#include "AssetImportUtils.h"
#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Factories/TextureCubeFactory.h"
#include "Misc/AutomationTest.h"
#include "Tests/LeonEdTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

// The cube map importer (UTextureCubeFactory, Docs/PLANS/ps2-polish.md P8): a long-lat Radiance HDR image becomes six
// tone-mapped sRGB faces, each face looking where UTextureCube::GetFaceBasis says, the same bytes every time.

namespace
{

	constexpr int32 PanoramaWidth = 64;
	constexpr int32 PanoramaHeight = 32;

	/** A Radiance file (flat RGBE scanlines, the top row first) of the panorama's texels, RGBE(X, Y) each. */
	template <typename FunctionType>
	TArray<uint8> MakeHdr(FunctionType&& RGBE)
	{
		const FString Header =
			FString::Printf("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n", PanoramaHeight, PanoramaWidth);
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(*Header), Header.Len());
		for (int32 Y = 0; Y < PanoramaHeight; ++Y)
		{
			for (int32 X = 0; X < PanoramaWidth; ++X)
			{
				const FColor Texel = RGBE(X, Y);
				Bytes.Add(Texel.R);
				Bytes.Add(Texel.G);
				Bytes.Add(Texel.B);
				Bytes.Add(Texel.A);
			}
		}
		return Bytes;
	}

	/** The byte a linear radiance becomes: the ACES curve, then sRGB. */
	uint8 DisplayByte(float Radiance)
	{
		const float Mapped = UTextureCubeFactory::ToneMapACES(Radiance);
		const float Encoded =
			Mapped <= 0.0031308f ? Mapped * 12.92f : (1.055f * FMath::Pow(Mapped, 1.0f / 2.4f)) - 0.055f;
		return uint8(FMath::Clamp(FMath::RoundToInt(Encoded * 255.0f), 0, 255));
	}

	/** A face's texel (Column, Row; the bottom row 0), RGB. */
	FColor FaceTexel(const UTextureCube& Cube, ECubeFace Face, int32 Column, int32 Row)
	{
		const UTexture2D* Texture = Cube.GetFace(Face);
		const FByteBulkData& Bulk = Texture->GetPlatformData().Mips[0].BulkData;
		const uint8* Texels = static_cast<const uint8*>(Bulk.LockReadOnly());
		const uint8* Texel = Texels + (((Row * Texture->GetSizeX()) + Column) * 4);
		const FColor Color(Texel[0], Texel[1], Texel[2], Texel[3]);
		Bulk.Unlock();
		return Color;
	}

	UTextureCube* Import(const FString& File, const TCHAR* Name, const TMap<FString, FString>& Settings)
	{
		UTextureCubeFactory* Factory = NewObject<UTextureCubeFactory>();
		(void)Factory->ApplyImportSettings(Settings);
		UPackage* Package = CreatePackage(*(FString(LeonEdTest::Root) + Name));
		return Cast<UTextureCube>(
			UFactory::StaticImportObject(nullptr, Package, FName(Name), RF_Public | RF_Standalone, File, Factory));
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdTextureCubeFaceBasisTest, "System.LeonEd.Factories.TextureCube.FaceBasis",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdTextureCubeFaceBasisTest::RunTest(const FString& Parameters)
{
	// Each face's centre is its axis; right and up as a view looking at it sees them (UE's right = up ^ forward), the
	// top and bottom faces as if the view pitched up or down from +X.
	struct FExpected
	{
		ECubeFace Face;
		FVector Forward;
		FVector Right;
		FVector Up;
	};
	const FExpected Expected[] = {
		{ECubeFace::PosX, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1)},
		{ECubeFace::NegX, FVector(-1, 0, 0), FVector(0, -1, 0), FVector(0, 0, 1)},
		{ECubeFace::PosY, FVector(0, 1, 0), FVector(-1, 0, 0), FVector(0, 0, 1)},
		{ECubeFace::NegY, FVector(0, -1, 0), FVector(1, 0, 0), FVector(0, 0, 1)},
		{ECubeFace::PosZ, FVector(0, 0, 1), FVector(0, 1, 0), FVector(-1, 0, 0)},
		{ECubeFace::NegZ, FVector(0, 0, -1), FVector(0, 1, 0), FVector(1, 0, 0)},
	};
	for (const FExpected& Face : Expected)
	{
		FVector Forward;
		FVector Right;
		FVector Up;
		UTextureCube::GetFaceBasis(Face.Face, Forward, Right, Up);
		const FString Name = UTextureCube::GetFaceName(Face.Face);
		TestTrue(*(Name + TEXT(": forward")), Forward.Equals(Face.Forward));
		TestTrue(*(Name + TEXT(": right")), Right.Equals(Face.Right));
		TestTrue(*(Name + TEXT(": up")), Up.Equals(Face.Up));
		TestTrue(
			*(Name + TEXT(": the centre")), UTextureCube::GetFaceDirection(Face.Face, 0.5f, 0.5f).Equals(Face.Forward));
		TestTrue(*(Name + TEXT(": the top right corner")),
			UTextureCube::GetFaceDirection(Face.Face, 1.0f, 1.0f)
				.Equals((Face.Forward + Face.Right + Face.Up).GetSafeNormal(), 1.0e-5f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdTextureCubeImportTest, "System.LeonEd.Factories.TextureCube.Import",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdTextureCubeImportTest::RunTest(const FString& Parameters)
{
	// A long-lat .hdr imports as a UTextureCube: six clamped sRGB faces of CubeFaceSize, each texel the ACES tone
	// mapping of the radiance along its direction (the panorama's middle column is +X, its top row the zenith); the
	// exposure scales the radiance first; the horizon's colour is the average near it. Importing over itself saves
	// the same bytes (gate G5).
	TestEqual("ACES of 0", UTextureCubeFactory::ToneMapACES(0.0f), 0.0f);
	TestEqual("ACES of 1", UTextureCubeFactory::ToneMapACES(1.0f), 2.54f / 3.16f, 1.0e-5f);
	TestTrue("ACES saturates", UTextureCubeFactory::ToneMapACES(1000.0f) > 0.99f);

	LeonEdTest::FScopedTestContent Content;
	// The sky (the upper half) warm on the +X quarter of the yaws and blue elsewhere; the ground (the lower half)
	// grey. RGBE: a colour's largest channel 1.0 is 128 with the exponent 129.
	const FColor Warm(128, 64, 32, 129);
	const FColor Blue(32, 64, 128, 129);
	const FColor Grey(128, 128, 128, 128);
	const FString Regions = LeonEdTest::WriteSource(TEXT("Sky/Regions.hdr"),
		MakeHdr(
			[&](int32 X, int32 Y)
			{
				const bool bPlusX = X >= (PanoramaWidth * 3) / 8 && X < (PanoramaWidth * 5) / 8;
				return Y < PanoramaHeight / 2 ? (bPlusX ? Warm : Blue) : Grey;
			}));
	TMap<FString, FString> Settings;
	Settings.Add(TEXT("CubeFaceSize"), TEXT("16"));
	UTextureCube* Cube = Import(Regions, TEXT("T_Regions"), Settings);
	if (!TestNotNull("Imported", Cube) || !TestTrue("Six faces", Cube->HasValidFaces()))
	{
		return false;
	}
	TestEqual("The faces' size", Cube->GetSizeX(), 16);
	for (int32 Face = 0; Face < int32(ECubeFace::Num); ++Face)
	{
		const UTexture2D* Texture = Cube->GetFace(ECubeFace(Face));
		TestTrue("A face: RGBA8, sRGB, clamped, the cube's subobject",
			Texture->GetPixelFormat() == PF_R8G8B8A8 && Texture->SRGB != 0 &&
				Texture->AddressX == ETextureAddress::Clamp && Texture->AddressY == ETextureAddress::Clamp &&
				Texture->GetOuter() == Cube &&
				Texture->GetFName() == FName(UTextureCube::GetFaceName(ECubeFace(Face))));
	}
	const FColor WarmByte(DisplayByte(1.0f), DisplayByte(0.5f), DisplayByte(0.25f));
	const FColor BlueByte(DisplayByte(0.25f), DisplayByte(0.5f), DisplayByte(1.0f));
	const FColor GreyByte(DisplayByte(0.5f), DisplayByte(0.5f), DisplayByte(0.5f));
	const auto Same = [](const FColor& A, const FColor& B)
	{ return A.R == B.R && A.G == B.G && A.B == B.B && A.A == 255; };
	// Rows 12 and 3 of 16 look 29 degrees above and below the horizon; column 8 about ahead.
	TestTrue("+X above the horizon: the warm quarter", Same(FaceTexel(*Cube, ECubeFace::PosX, 8, 12), WarmByte));
	TestTrue("+X below it: the ground", Same(FaceTexel(*Cube, ECubeFace::PosX, 8, 3), GreyByte));
	TestTrue("-X above the horizon: blue", Same(FaceTexel(*Cube, ECubeFace::NegX, 8, 12), BlueByte));
	TestTrue("+Y above the horizon: blue", Same(FaceTexel(*Cube, ECubeFace::PosY, 8, 12), BlueByte));
	TestTrue("-Z: the ground", Same(FaceTexel(*Cube, ECubeFace::NegZ, 8, 8), GreyByte));
	// +Z's bottom row looks toward +X (its up is -X), just above the warm quarter's horizon.
	TestTrue("+Z's bottom toward +X: warm", Same(FaceTexel(*Cube, ECubeFace::PosZ, 8, 0), WarmByte));
	TestTrue("+Z's top toward -X: blue", Same(FaceTexel(*Cube, ECubeFace::PosZ, 8, 15), BlueByte));

	// A uniform sky: every texel the one byte, the horizon that byte; one stop of exposure doubles the radiance.
	const FString Uniform = LeonEdTest::WriteSource(
		TEXT("Sky/Uniform.hdr"), MakeHdr([](int32, int32) { return FColor(128, 128, 128, 129); }));
	UTextureCube* Flat = Import(Uniform, TEXT("T_Uniform"), Settings);
	if (!TestNotNull("The uniform one imported", Flat))
	{
		return false;
	}
	const uint8 One = DisplayByte(1.0f);
	TestTrue("Every face the tone-mapped 1.0",
		Same(FaceTexel(*Flat, ECubeFace::NegY, 0, 0), FColor(One, One, One)) &&
			Same(FaceTexel(*Flat, ECubeFace::PosZ, 15, 15), FColor(One, One, One)));
	TestTrue("The horizon's colour", Flat->HorizonColor.Equals(FLinearColor(One / 255.0f, One / 255.0f, One / 255.0f)));
	TMap<FString, FString> Brighter = Settings;
	Brighter.Add(TEXT("ExposureBias"), TEXT("1"));
	UTextureCube* Exposed = Import(Uniform, TEXT("T_Exposed"), Brighter);
	const uint8 Two = DisplayByte(2.0f);
	TestTrue("ExposureBias 1: the tone-mapped 2.0",
		Exposed != nullptr && Same(FaceTexel(*Exposed, ECubeFace::PosX, 3, 3), FColor(Two, Two, Two)));
	TestTrue("Import data",
		Exposed != nullptr && Exposed->AssetImportData != nullptr &&
			Exposed->AssetImportData->ImportSettings.FindRef(TEXT("ExposureBias")) == FString(TEXT("1")));

	// Imported over itself: the same bytes.
	UPackage* Package = Cube->GetOutermost();
	TestTrue("Saved", FAssetImportUtils::SavePackage(Package, Cube));
	const FString Saved = FAssetImportUtils::GetPackageFilename(Package->GetName());
	const TArray<uint8> FirstBytes = LeonEdTest::ReadBytes(Saved);
	UTextureCube* Again = Import(Regions, TEXT("T_Regions"), Settings);
	TestTrue("Reimported over itself", Again == Cube);
	TestTrue("Saved again", FAssetImportUtils::SavePackage(Package, Cube));
	TestTrue("The same bytes", FirstBytes.Num() > 0 && FirstBytes == LeonEdTest::ReadBytes(Saved));

	// A face size the GS cannot sample fails the import.
	TMap<FString, FString> Bad;
	Bad.Add(TEXT("CubeFaceSize"), TEXT("100"));
	AddExpectedError(TEXT("CubeFaceSize=100"));
	AddExpectedError(TEXT("TextureCubeFactory failed to import"));
	TestNull("A face of 100 texels", Import(Regions, TEXT("T_Bad"), Bad));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
