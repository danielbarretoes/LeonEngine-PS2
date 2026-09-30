#include "Factories/TextureCubeFactory.h"

#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "LeonEdLog.h"

namespace
{

	/** A linear value in 0..1 encoded as sRGB (IEC 61966-2-1). */
	float EncodeSRGB(float Linear)
	{
		return Linear <= 0.0031308f ? Linear * 12.92f : (1.055f * FMath::Pow(Linear, 1.0f / 2.4f)) - 0.055f;
	}

	/** A long-lat panorama of linear RGB, the top row first, sampled bilinearly along a direction. */
	struct FLongLatImage
	{
		int32 Width = 0;
		int32 Height = 0;
		const float* Texels = nullptr;

		FVector Texel(int32 X, int32 Y) const
		{
			const float* Value = Texels + ((int64(Y) * Width) + X) * 3;
			return FVector(Value[0], Value[1], Value[2]);
		}

		/** The radiance along a unit direction: the columns wrap around, the rows clamp at the poles. */
		FVector Sample(const FVector& Direction) const
		{
			const float Yaw = FMath::Atan2(Direction.Y, Direction.X);
			const float Pitch = FMath::Asin(FMath::Clamp(Direction.Z, -1.0f, 1.0f));
			const float X = (((Yaw / (2.0f * PI)) + 0.5f) * float(Width)) - 0.5f;
			const float Y = ((0.5f - (Pitch / PI)) * float(Height)) - 0.5f;
			const int32 X0 = FMath::FloorToInt(X);
			const int32 Y0 = FMath::FloorToInt(Y);
			const float FracX = X - float(X0);
			const float FracY = Y - float(Y0);
			const int32 Left = ((X0 % Width) + Width) % Width;
			const int32 Right = (Left + 1) % Width;
			const int32 Top = FMath::Clamp(Y0, 0, Height - 1);
			const int32 Bottom = FMath::Clamp(Y0 + 1, 0, Height - 1);
			const FVector Upper = FMath::Lerp(Texel(Left, Top), Texel(Right, Top), FracX);
			const FVector Lower = FMath::Lerp(Texel(Left, Bottom), Texel(Right, Bottom), FracX);
			return FMath::Lerp(Upper, Lower, FracY);
		}
	};

} // namespace

UTextureCubeFactory::UTextureCubeFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UTextureCube::StaticClass();
	Formats.Add(TEXT("hdr;Radiance HDR image (long-lat)"));
	bEditorImport = 1;
}

float UTextureCubeFactory::ToneMapACES(float Value)
{
	const float X = FMath::Max(Value, 0.0f);
	return FMath::Clamp((X * ((2.51f * X) + 0.03f)) / ((X * ((2.43f * X) + 0.59f)) + 0.14f), 0.0f, 1.0f);
}

bool UTextureCubeFactory::FillCube(
	UTextureCube& Cube, int32 Width, int32 Height, const TArray<float>& Radiance, FString& OutError) const
{
	const int32 Size = CubeFaceSize;
	if (Size < 8 || Size > 256 || !FMath::IsPowerOfTwo(Size))
	{
		OutError = FString::Printf("CubeFaceSize=%d is not a power of two from 8 to 256", Size);
		return false;
	}
	if (Width < 2 || Height < 2 || Radiance.Num() != Width * Height * 3)
	{
		OutError = FString::Printf("a %d x %d panorama", Width, Height);
		return false;
	}
	const FLongLatImage Image{Width, Height, Radiance.GetData()};
	const float Exposure = FMath::Pow(2.0f, ExposureBias);
	const float HorizonSin = FMath::Sin(FMath::DegreesToRadians(FMath::Clamp(HorizonDegrees, 0.0f, 90.0f)));
	FVector HorizonSum = FVector::ZeroVector;
	int32 HorizonTexels = 0;

	// The faces, reused by name on a reimport (the same subobjects, the same bytes).
	TArray<UTexture2D*> Faces;
	TArray<uint8> Texels;
	Texels.SetNumUninitialized(Size * Size * 4);
	for (int32 FaceIndex = 0; FaceIndex < int32(ECubeFace::Num); ++FaceIndex)
	{
		const ECubeFace Face = ECubeFace(FaceIndex);
		const bool bSide = Face != ECubeFace::PosZ && Face != ECubeFace::NegZ;
		for (int32 Row = 0; Row < Size; ++Row)
		{
			for (int32 Column = 0; Column < Size; ++Column)
			{
				// Four samples a texel, a quarter of it apart, averaged.
				FVector Sum = FVector::ZeroVector;
				for (int32 Sub = 0; Sub < 4; ++Sub)
				{
					const float U = (float(Column) + 0.25f + (0.5f * float(Sub & 1))) / float(Size);
					const float V = (float(Row) + 0.25f + (0.5f * float(Sub >> 1))) / float(Size);
					Sum += Image.Sample(UTextureCube::GetFaceDirection(Face, U, V));
				}
				const FVector Linear = Sum * (0.25f * Exposure);
				uint8* Texel = &Texels[((Row * Size) + Column) * 4];
				for (int32 Channel = 0; Channel < 3; ++Channel)
				{
					const float Display = EncodeSRGB(ToneMapACES(Linear[Channel]));
					Texel[Channel] = uint8(FMath::Clamp(FMath::RoundToInt(Display * 255.0f), 0, 255));
				}
				Texel[3] = 255;
				const FVector Direction = UTextureCube::GetFaceDirection(
					Face, (float(Column) + 0.5f) / float(Size), (float(Row) + 0.5f) / float(Size));
				if (bSide && Direction.Z >= 0.0f && Direction.Z <= HorizonSin)
				{
					HorizonSum += FVector(float(Texel[0]), float(Texel[1]), float(Texel[2]));
					++HorizonTexels;
				}
			}
		}
		const FName FaceName(UTextureCube::GetFaceName(Face));
		UTexture2D* Texture = FindObjectFast<UTexture2D>(&Cube, FaceName);
		if (Texture == nullptr)
		{
			Texture =
				NewObject<UTexture2D>(&Cube, FaceName, Cube.HasAnyFlags(RF_Transient) ? RF_Transient : RF_NoFlags);
		}
		Texture->SRGB = 1;
		// A face's edge meets the next face: its texels must not wrap to the opposite edge.
		Texture->AddressX = ETextureAddress::Clamp;
		Texture->AddressY = ETextureAddress::Clamp;
		(void)Texture->SetPlatformData(Size, Size, PF_R8G8B8A8, Texels.GetData());
		Faces.Add(Texture);
	}
	Cube.SRGB = 1;
	Cube.Faces = Faces;
	const FVector Horizon = HorizonTexels > 0 ? HorizonSum / (255.0f * float(HorizonTexels)) : FVector::OneVector;
	Cube.HorizonColor = FLinearColor(Horizon.X, Horizon.Y, Horizon.Z, 1.0f);
	return true;
}

UObject* UTextureCubeFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, bool& bOutOperationCanceled)
{
	(void)InClass;
	(void)Context;
	(void)Type;
	bOutOperationCanceled = false;
	int32 Width = 0;
	int32 Height = 0;
	TArray<float> Radiance;
	FString Error;
	if (!UTextureFactory::DecodeHDRImage(Buffer, BufferEnd - Buffer, Width, Height, Radiance, Error))
	{
		UE_LOG(LogLeonEd, Error, "TextureCubeFactory: cannot decode '%s' (%s)", *CurrentFilename, *Error);
		return nullptr;
	}
	UTextureCube* Cube = CreateOrOverwriteAsset<UTextureCube>(InParent, InName, Flags);
	if (Cube == nullptr)
	{
		return nullptr;
	}
	if (!FillCube(*Cube, Width, Height, Radiance, Error))
	{
		UE_LOG(LogLeonEd, Error, "TextureCubeFactory: cannot import '%s' (%s)", *CurrentFilename, *Error);
		return nullptr;
	}
	UpdateAssetImportData(Cube, CurrentFilename);
	Buffer = BufferEnd;
	return Cube;
}

bool UTextureCubeFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	return FactoryCanReimport(Obj, OutFilenames);
}

void UTextureCubeFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	FactorySetReimportPaths(Obj, NewReimportPaths);
}

EReimportResult::Type UTextureCubeFactory::Reimport(UObject* Obj)
{
	return FactoryReimport(Obj);
}

void UTextureCubeFactory::GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const
{
	FactoryGetAdditionalReimportedObjects(OutObjects);
}
