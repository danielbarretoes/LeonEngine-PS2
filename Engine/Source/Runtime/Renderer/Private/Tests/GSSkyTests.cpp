#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "GS/GSSceneRenderer.h"
#include "GSReferenceRasterizer.h"
#include "GameFramework/WorldSettings.h"
#include "LPS2Mesh.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Primitives.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "SkyBoxGeometry.h"
#include "Templates/UniquePtr.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// The sky of Docs/PLANS/ps2-polish.md P8: the sky box's geometry, a frame of a world with a sky drawn by the GS scene
// renderer on the reference rasterizer (the faces where the view's yaw and pitch put them, the world over it, no batch
// through the clipper, VU1's batches the same pixels), and the fog's colour from the sky's horizon.

namespace
{

	constexpr int32 FrameWidth = 640;
	constexpr int32 FrameHeight = 448;
	constexpr uint32 ArenaFirstBlock = 280 * 32;
	constexpr uint32 ArenaBlocks = (512 - 280) * 32;
	/** A face's texels: four quadrants of its colour, brighter to the right and to the top. */
	constexpr int32 FaceSize = 16;

	/** A 640 x 448 PSMCT32 frame at FBP 0, its PSMZ24 Z buffer after it. */
	FGSDrawEnvironment MakeEnvironment()
	{
		FGSDrawEnvironment Environment;
		Environment.Frame.FBP = 0;
		Environment.Frame.FBW = FrameWidth / 64;
		Environment.Frame.PSM = EGSPixelFormat::PSMCT32;
		Environment.ZBuf.ZBP = 140;
		Environment.ZBuf.PSM = EGSPixelFormat::PSMZ24;
		Environment.Width = FrameWidth;
		Environment.Height = FrameHeight;
		return Environment;
	}

	/**
	 * The frame List draws on Rasterizer, whose local memory keeps what the frames before uploaded (the textures the
	 * cache keeps resident, as the GS does).
	 */
	TArray<FColor> Rasterize(FGSReferenceRasterizer& Rasterizer, const FGSCommandList& List)
	{
		Rasterizer.Execute(List);
		return Rasterizer.ReadFrame(MakeEnvironment().Frame, FrameWidth, FrameHeight);
	}

	/** Each face's colour (its bottom-left quadrant; the others add 60 to red to the right and to green at the top). */
	const FColor FaceColors[6] = {FColor(40, 40, 200), FColor(40, 40, 120), FColor(120, 20, 60), FColor(60, 20, 20),
		FColor(20, 120, 180), FColor(20, 60, 20)};

	FColor QuadrantColor(int32 Face, bool bRight, bool bTop)
	{
		FColor Color = FaceColors[Face];
		Color.R = uint8(Color.R + (bRight ? 60 : 0));
		Color.G = uint8(Color.G + (bTop ? 60 : 0));
		return Color;
	}

	/** A cube map of six FaceSize faces in FaceColors' quadrants, clamped, its horizon HorizonColor. */
	UTextureCube* MakeTestCube(const FLinearColor& HorizonColor)
	{
		UTextureCube* Cube = NewObject<UTextureCube>();
		for (int32 Face = 0; Face < 6; ++Face)
		{
			TArray<uint8> Texels;
			Texels.SetNumUninitialized(FaceSize * FaceSize * 4);
			// Bottom row first.
			for (int32 Row = 0; Row < FaceSize; ++Row)
			{
				for (int32 Column = 0; Column < FaceSize; ++Column)
				{
					const FColor Color = QuadrantColor(Face, Column >= FaceSize / 2, Row >= FaceSize / 2);
					uint8* Texel = &Texels[((Row * FaceSize) + Column) * 4];
					Texel[0] = Color.R;
					Texel[1] = Color.G;
					Texel[2] = Color.B;
					Texel[3] = 255;
				}
			}
			UTexture2D* Texture = NewObject<UTexture2D>(Cube);
			Texture->AddressX = ETextureAddress::Clamp;
			Texture->AddressY = ETextureAddress::Clamp;
			(void)Texture->SetPlatformData(FaceSize, FaceSize, PF_R8G8B8A8, Texels.GetData());
			Cube->Faces.Add(Texture);
		}
		Cube->HorizonColor = HorizonColor;
		return Cube;
	}

	/** A world with its world settings, the sky set. */
	AWorldSettings& SetSky(UWorld& World, UTextureCube* Sky)
	{
		AWorldSettings* WorldSettings = World.SpawnActor<AWorldSettings>();
		World.PersistentLevel->SetWorldSettings(WorldSettings);
		WorldSettings->SkySettings.SkyCubemap = Sky;
		return *WorldSettings;
	}

	/** A camera at the origin looking along Rotation, 60 degrees vertically, near 10 cm, far 1 km. */
	UCameraComponent& MakeCamera(const FRotator& Rotation)
	{
		UCameraComponent& Camera = *NewObject<UCameraComponent>();
		Camera.SetPerspective(60.0f, float(FrameWidth) / float(FrameHeight), 10.0f, 100000.0f);
		Camera.SetMode(ECameraMode::FreeLook);
		Camera.SetEyeLocation(FVector::ZeroVector);
		Camera.SetViewRotation(Rotation);
		return Camera;
	}

	/** Renders World's scene from Camera into a new list (the environment appended first). */
	FFrameStats RenderFrame(UWorld& World, UCameraComponent& Camera, FGSSceneRenderer& Renderer, FGSCommandList& List)
	{
		World.SendAllEndOfFrameUpdates();
		FSceneViewFamily Family(
			FSceneViewFamily::ConstructionValues(FrameWidth, FrameHeight, World.Scene, FEngineShowFlags()));
		const FSceneView View(FSceneView::FromCamera(Family, Camera));
		Family.Views.Add(&View);
		const FGSDrawEnvironment Environment = MakeEnvironment();
		List.Reset();
		Environment.Append(List);
		Renderer.Render(Family, Environment, List);
		return Renderer.GetFrameStats();
	}

	FColor PixelAt(const TArray<FColor>& Pixels, int32 X, int32 Y)
	{
		return Pixels[(Y * FrameWidth) + X];
	}

	bool SameColor(const FColor& A, const FColor& B)
	{
		return FMath::Abs(int32(A.R) - int32(B.R)) <= 2 && FMath::Abs(int32(A.G) - int32(B.G)) <= 2 &&
			FMath::Abs(int32(A.B) - int32(B.B)) <= 2;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSkyBoxGeometryTest, "System.Renderer.GS.Sky.BoxGeometry",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSkyBoxGeometryTest::RunTest(const FString& Parameters)
{
	// The box: one section a face (its material slot the face), every batch VU1-sized and small (its sphere within 14
	// degrees of its centre from the eye), the same bytes every build; its radius between the near and far planes.
	FLPS2Mesh Mesh;
	if (!TestTrue("Built", FSkyBoxGeometry::BuildMesh(Mesh)))
	{
		return false;
	}
	TestEqual("A section a face", Mesh.GetNumSections(), 6);
	TestEqual("Batches", Mesh.GetNumBatches(), 6 * FSkyBoxGeometry::BatchesPerFace);
	TestEqual(
		"Triangles", Mesh.GetNumTriangles(), 6 * FSkyBoxGeometry::QuadsPerSide * FSkyBoxGeometry::QuadsPerSide * 2);
	float WidestDegrees = 0.0f;
	for (int32 Section = 0; Section < Mesh.GetNumSections(); ++Section)
	{
		TestEqual("The face's slot", int32(Mesh.GetSection(Section).MaterialIndex), Section);
	}
	for (int32 Index = 0; Index < Mesh.GetNumBatches(); ++Index)
	{
		const FLPS2Batch& Batch = Mesh.GetBatch(Index);
		const FVector Center(Batch.BoundsCenter[0], Batch.BoundsCenter[1], Batch.BoundsCenter[2]);
		WidestDegrees =
			FMath::Max(WidestDegrees, FMath::RadiansToDegrees(FMath::Asin(Batch.BoundsRadius / Center.Size())));
	}
	UE_LOG(LogTemp, Display, "%s",
		*FString::Printf("Sky box: %d batches, %d vertices, %d bytes, the widest %.1f degrees", Mesh.GetNumBatches(),
			Mesh.GetNumVertices(), Mesh.GetData().Num(), double(WidestDegrees)));
	TestTrue("Every batch small", WidestDegrees > 0.0f && WidestDegrees <= 14.0f);
	FLPS2Mesh Again;
	TestTrue("The same bytes", FSkyBoxGeometry::BuildMesh(Again) && Again.GetData() == Mesh.GetData());

	const FPerspectiveMatrix Projection(
		FMath::DegreesToRadians(30.0f), FMath::DegreesToRadians(30.0f), 1.0f, 1.0f, 10.0f, 100000.0f);
	TestEqual("The radius between near 10 cm and far 1 km", FSkyBoxGeometry::GetRadius(Projection), 1000.0f, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSkyOrientationTest, "System.Renderer.GS.Sky.Orientation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSkyOrientationTest::RunTest(const FString& Parameters)
{
	// A reference frame for each of six views, a known yaw and pitch each: the face the view looks at fills the
	// frame's centre, its quadrants where the face's right and up put them (UTextureCube::GetFaceBasis). A world cube
	// 30 m away, farther than the sky box, shows over the sky: the sky wrote no Z and drew first. No batch of the sky
	// goes through the clipper, from these views or 48 others; VU1's recorded batches draw the same pixels.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	(void)SetSky(World, MakeTestCube(FLinearColor::White));
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	const TUniquePtr<FGSReferenceRasterizer> GS = MakeUnique<FGSReferenceRasterizer>();

	struct FView
	{
		const TCHAR* Name;
		FRotator Rotation;
		ECubeFace Face;
	};
	const FView Views[] = {
		{TEXT("Yaw 0: +X"), FRotator(0.0f, 0.0f, 0.0f), ECubeFace::PosX},
		{TEXT("Yaw 90: +Y"), FRotator(0.0f, 90.0f, 0.0f), ECubeFace::PosY},
		{TEXT("Yaw 180: -X"), FRotator(0.0f, 180.0f, 0.0f), ECubeFace::NegX},
		{TEXT("Yaw -90: -Y"), FRotator(0.0f, -90.0f, 0.0f), ECubeFace::NegY},
		{TEXT("Pitch 90: +Z"), FRotator(89.9f, 0.0f, 0.0f), ECubeFace::PosZ},
		{TEXT("Pitch -90: -Z"), FRotator(-89.9f, 0.0f, 0.0f), ECubeFace::NegZ},
	};
	FGSCommandList List;
	for (const FView& View : Views)
	{
		UCameraComponent& Camera = MakeCamera(View.Rotation);
		const FFrameStats Stats = RenderFrame(World, Camera, Renderer, List);
		const TArray<FColor> Pixels = Rasterize(*GS, List);
		const int32 Face = int32(View.Face);
		// A quarter of the frame from the centre: well inside the face (it spans 90 degrees, the view 60 high).
		const int32 Left = (FrameWidth / 2) - 80;
		const int32 Right = (FrameWidth / 2) + 80;
		const int32 Top = (FrameHeight / 2) - 80;
		const int32 Bottom = (FrameHeight / 2) + 80;
		TestTrue(*FString::Printf("%s: top left", View.Name),
			SameColor(PixelAt(Pixels, Left, Top), QuadrantColor(Face, false, true)));
		TestTrue(*FString::Printf("%s: top right", View.Name),
			SameColor(PixelAt(Pixels, Right, Top), QuadrantColor(Face, true, true)));
		TestTrue(*FString::Printf("%s: bottom left", View.Name),
			SameColor(PixelAt(Pixels, Left, Bottom), QuadrantColor(Face, false, false)));
		TestTrue(*FString::Printf("%s: bottom right", View.Name),
			SameColor(PixelAt(Pixels, Right, Bottom), QuadrantColor(Face, true, false)));
		TestEqual(*FString::Printf("%s: no batch clipped", View.Name), Stats.BatchesClipped, 0);
		TestTrue(*FString::Printf("%s: batches drawn", View.Name), Stats.BatchesOnEmitter > 0);
	}

	// The world over the sky: a white cube 30 m ahead, past the sky box (10 m).
	UStaticMesh* CubeMesh = NewObject<UStaticMesh>();
	(void)CubeMesh->BuildFromMeshData(MakeCube());
	AStaticMeshActor* Actor =
		World.SpawnActor<AStaticMeshActor>(FVector(3000.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f));
	UStaticMeshComponent& Component = *Actor->GetStaticMeshComponent();
	(void)Component.SetStaticMesh(CubeMesh);
	UMaterial* Material = NewObject<UMaterial>();
	Material->ShadingModel = MSM_Unlit;
	Material->BaseColor = FLinearColor::White;
	Component.SetMaterial(0, Material);
	Actor->SetActorScale3D(FVector(3.0f));
	UCameraComponent& Ahead = MakeCamera(FRotator(0.0f, 0.0f, 0.0f));
	const FFrameStats WithCube = RenderFrame(World, Ahead, Renderer, List);
	const TArray<FColor> Emitted = Rasterize(*GS, List);
	TestTrue("The cube over the sky", SameColor(PixelAt(Emitted, FrameWidth / 2, FrameHeight / 2), FColor::White));
	TestTrue("The sky around it",
		SameColor(PixelAt(Emitted, (FrameWidth / 2) + 120, (FrameHeight / 2) + 120), QuadrantColor(0, true, false)));
	// The sky's writes come first after the clear: Z masked (ZBUF with ZMSK) before any vertex of the frame's second
	// primitive run, and unmasked again for the world.
	int32 FirstMasked = INDEX_NONE;
	int32 FirstUnmasked = INDEX_NONE;
	const TArray<FGSRegisterWrite>& Writes = List.GetWrites();
	for (int32 Index = 0; Index < Writes.Num(); ++Index)
	{
		if (Writes[Index].Register == EGSRegister::ZBUF_1)
		{
			const bool bMasked = FGSZBuf::Decode(Writes[Index].Value).bMask;
			if (bMasked && FirstMasked == INDEX_NONE)
			{
				FirstMasked = Index;
			}
			if (!bMasked && FirstMasked != INDEX_NONE && FirstUnmasked == INDEX_NONE)
			{
				FirstUnmasked = Index;
			}
		}
	}
	TestTrue("The sky with Z masked, then the world with Z written",
		FirstMasked != INDEX_NONE && FirstUnmasked != INDEX_NONE && FirstMasked < FirstUnmasked);
	UE_LOG(LogTemp, Display, "%s",
		*FString::Printf("Sky frame: %d GS writes (%d KB of GIF from the EE), %d batches", WithCube.RegisterWrites,
			(WithCube.RegisterWrites * 16) / 1024, WithCube.BatchesOnEmitter));

	// VU1's batches: the sky's draws recorded, expanded by the C++ emitter to the same frame.
	Renderer.SetVertexBatches(true);
	FGSCommandList Recorded;
	const FFrameStats RecordedStats = RenderFrame(World, Ahead, Renderer, Recorded);
	TestTrue("The sky's batches on VU1", RecordedStats.BatchesOnVU1 > 0 && RecordedStats.BatchesOnEmitter == 0);
	TestTrue("The first draw is the sky's: unlit, textured, unfogged",
		Recorded.GetVertexDraws().Num() > 0 && !Recorded.GetVertexDraws()[0].bLit &&
			Recorded.GetVertexDraws()[0].bTextured && !Recorded.GetVertexDraws()[0].Fog.bEnabled);
	FGSCommandList Expanded;
	Expanded.AppendExpanded(Recorded, MakeEnvironment());
	const TArray<FColor> Drawn = Rasterize(*GS, Expanded);
	int32 NumDifferent = 0;
	for (int32 Index = 0; Index < Drawn.Num(); ++Index)
	{
		NumDifferent += Drawn[Index] == Emitted[Index] ? 0 : 1;
	}
	TestEqual("VU1's batches draw the same sky", NumDifferent, 0);
	Renderer.SetVertexBatches(false);

	// Any view: the sky's batches stay out of the clipper (the cube hidden: the world's batches may cross).
	Actor->SetActorHiddenInGame(true);
	int32 Clipped = 0;
	for (int32 Step = 0; Step < 48; ++Step)
	{
		const FRotator Rotation(float((Step * 37) % 180) - 90.0f, float(Step * 53), float((Step * 29) % 60) - 30.0f);
		UCameraComponent& Camera = MakeCamera(Rotation);
		Clipped += RenderFrame(World, Camera, Renderer, List).BatchesClipped;
	}
	TestEqual("No batch clipped from 48 other views", Clipped, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSkyFogColorTest, "System.Renderer.GS.Sky.FogColor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSkyFogColorTest::RunTest(const FString& Parameters)
{
	// With a sky the fog takes its horizon's colour (bInscatteringColorFromSky); without one, or with the flag off,
	// the fog's own.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AWorldSettings& WorldSettings = SetSky(World, MakeTestCube(FLinearColor(0.8f, 0.6f, 0.4f, 1.0f)));
	WorldSettings.FogSettings.bEnableFog = true;
	WorldSettings.FogSettings.FogInscatteringColor = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	UCameraComponent& Camera = MakeCamera(FRotator(0.0f, 0.0f, 0.0f));
	const auto FogColor = [&]()
	{
		FGSCommandList List;
		(void)RenderFrame(World, Camera, Renderer, List);
		for (const FGSRegisterWrite& Write : List.GetWrites())
		{
			if (Write.Register == EGSRegister::FOGCOL)
			{
				return FColor(
					uint8(Write.Value & 0xff), uint8((Write.Value >> 8) & 0xff), uint8((Write.Value >> 16) & 0xff));
			}
		}
		return FColor(0, 0, 0, 0);
	};
	TestTrue("The sky's horizon", FogColor() == FColor(204, 153, 102));
	WorldSettings.FogSettings.bInscatteringColorFromSky = false;
	TestTrue("The fog's own colour with the flag off", FogColor() == FColor(0, 0, 255));
	WorldSettings.FogSettings.bInscatteringColorFromSky = true;
	WorldSettings.SkySettings.SkyCubemap = nullptr;
	TestTrue("The fog's own colour without a sky", FogColor() == FColor(0, 0, 255));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
