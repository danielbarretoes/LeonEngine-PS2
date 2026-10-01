#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "DynamicRHI.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GS/GSSceneRenderer.h"
#include "GSCommandList.h"
#include "GSConformanceScenes.h"
#include "GSDebugDraw.h"
#include "GSEmulator/GSOpenGLEmulator.h"
#include "GSEmulator/PS2TexturePreview.h"
#include "GSReferenceRasterizer.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "LPS2Mesh.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "PalettedTexture.h"
#include "Primitives.h"
#include "SceneView.h"
#include "Tests/CanvasTestScene.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// The desktop's GS emulator against the reference rasterizer on every conformance scene (Docs/PLANS/ps2-gs-parity.md
// P4's gate), and on a frame of the GS scene renderer (Docs/PLANS/ps2-engine.md E2's). They need an OpenGL 3.3 context
// (a window): NonNullRHI, so LeonAutomationTests -nodisplay skips them.

namespace
{

	/**
	 * How far a channel may be from the reference, and how many pixels of a scene may be further: OpenGL covers a pixel
	 * by a sample 1/256 right of and below its center, so a center exactly on a shallow side or on a vertex may go the
	 * other way (StripsAndSprites' fan has 3 such pixels); a minified bilinear weight interpolated in floating point
	 * may land a level or three away (MipmapLod has 3 such pixels).
	 */
	constexpr int32 ChannelTolerance = 2;
	constexpr int32 MaxDifferentPixels = 8;

	/**
	 * A scene frame's tolerance: one step of the 16-bit frame's 5-bit channels, and a few pixels beyond it. The
	 * emulator interpolates the texture coordinates and Z in floating point, so a far texel or a bilinear weight can
	 * round to the next step and the faces that share an edge can trade a pixel there.
	 */
	constexpr int32 SceneChannelTolerance = 8;
	constexpr int32 SceneMaxDifferentPixels = 64;

	/** A window with an OpenGL context and the RHI on it, for the test's lifetime. */
	class FScopedGLContext
	{
	public:
		FScopedGLContext()
		{
			Application.Reset(FPlatformApplicationMisc::CreateApplication());
			if (!Application)
			{
				return;
			}
			Window = Application->MakeWindow();
			if (!Window->Create(FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight, "GS emulator test"))
			{
				Window.Reset();
				return;
			}
			bValid = RHIInit(Window->GetRHIProcAddressLoader());
		}

		~FScopedGLContext()
		{
			if (bValid)
			{
				RHIExit();
			}
			if (Window)
			{
				Window->Destroy();
			}
		}

		FScopedGLContext(const FScopedGLContext&) = delete;
		FScopedGLContext& operator=(const FScopedGLContext&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return bValid;
		}

	private:
		TUniquePtr<GenericApplication> Application;
		TSharedPtr<FGenericWindow> Window;
		bool bValid = false;
	};

	/** The pixels further than Tolerance from the reference in a channel; logs them with the worst channel. */
	int32 CountDifferentPixels(
		const TCHAR* Name, const TArray<FColor>& Emulated, const TArray<FColor>& Expected, int32 Width, int32 Tolerance)
	{
		int32 NumDifferent = 0;
		int32 WorstChannel = 0;
		FString FirstDifference;
		for (int32 Index = 0; Index < Expected.Num() && Index < Emulated.Num(); ++Index)
		{
			const FColor& A = Emulated[Index];
			const FColor& B = Expected[Index];
			const int32 Difference =
				FMath::Max(FMath::Max(FMath::Abs(int32(A.R) - int32(B.R)), FMath::Abs(int32(A.G) - int32(B.G))),
					FMath::Max(FMath::Abs(int32(A.B) - int32(B.B)), FMath::Abs(int32(A.A) - int32(B.A))));
			WorstChannel = FMath::Max(WorstChannel, Difference);
			if (Difference > Tolerance)
			{
				if (NumDifferent == 0)
				{
					FirstDifference = FString::Printf("(%d, %d): emulated %d %d %d %d, reference %d %d %d %d",
						Index % Width, Index / Width, A.R, A.G, A.B, A.A, B.R, B.G, B.B, B.A);
				}
				++NumDifferent;
			}
		}
		if (Emulated.Num() != Expected.Num())
		{
			NumDifferent = FMath::Max(Emulated.Num(), Expected.Num());
		}
		UE_LOG(LogTemp, Display, "%s",
			*FString::Printf("%s: %d pixel(s) beyond %d, worst channel %d%s%s", Name, NumDifferent, Tolerance,
				WorstChannel, NumDifferent > 0 ? ", first " : "", *FirstDifference));
		return NumDifferent;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSEmulatorConformanceTest, "System.Renderer.GSEmulator.Conformance",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::SmokeFilter)

bool FGSEmulatorConformanceTest::RunTest(const FString& Parameters)
{
	FScopedGLContext Context;
	if (!Context.IsValid())
	{
		AddError("No OpenGL context (a display is needed; run LeonAutomationTests with -nodisplay to skip this test)");
		return false;
	}
	FGSOpenGLEmulator Emulator;
	if (!TestTrue("The emulator starts", Emulator.Initialize(FPaths::Combine(FPaths::EngineDir(), TEXT("Shaders")))))
	{
		return false;
	}
	for (const FGSConformanceScene& Scene : GSConformance::GetScenes())
	{
		FGSCommandList List;
		Scene.Build(List);
		Emulator.Execute(List);
		const TArray<FColor> Emulated =
			Emulator.ReadFrame(int32(GSConformance::FrameWidth), int32(GSConformance::FrameHeight));
		FGSReferenceRasterizer Reference;
		Reference.Execute(List);
		const TArray<FColor> Expected = Reference.ReadFrame(
			GSConformance::MakeFrame(Scene.FrameFormat), GSConformance::FrameWidth, GSConformance::FrameHeight);

		const int32 NumDifferent = CountDifferentPixels(
			*FString(Scene.Name), Emulated, Expected, int32(GSConformance::FrameWidth), ChannelTolerance);
		TestTrue(*FString::Printf("%s within the tolerance", Scene.Name), NumDifferent <= MaxDifferentPixels);
	}
	Emulator.Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSEmulatorSceneFrameTest, "System.Renderer.GSEmulator.SceneFrame",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::SmokeFilter)

bool FGSEmulatorSceneFrameTest::RunTest(const FString& Parameters)
{
	// A frame of the GS scene renderer in the desktop's environment (16-bit dithered colour, 24-bit Z, the texture
	// arena, the PS2 cook's textures: PSMT8 / PSMT4 with their mip chain, sampled trilinear with the LOD from Q):
	// textured and flat cubes under a sun and a point light, a translucent one in front, a floor clipped by the near
	// plane. The floor is Static with baked vertex colours (a gradient, N22); the cubes are Movable, lit per frame. The
	// emulator must draw what the reference draws from the same list.
	FScopedGLContext Context;
	if (!Context.IsValid())
	{
		AddError("No OpenGL context (a display is needed; run LeonAutomationTests with -nodisplay to skip this test)");
		return false;
	}
	FGSOpenGLEmulator Emulator;
	if (!TestTrue("The emulator starts", Emulator.Initialize(FPaths::Combine(FPaths::EngineDir(), TEXT("Shaders")))))
	{
		return false;
	}

	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	UStaticMesh* Cube = NewObject<UStaticMesh>();
	(void)Cube->BuildFromMeshData(MakeCube());
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineMaterials/T_Default_D.T_Default_D"));
	if (!TestNotNull("T_Default_D", Texture))
	{
		return false;
	}
	UMaterial* Textured = NewObject<UMaterial>();
	Textured->BaseColorMap = Texture;
	UMaterial* Flat = NewObject<UMaterial>();
	Flat->BaseColor = FLinearColor(0.8f, 0.3f, 0.2f, 1.0f);
	UMaterial* Glass = NewObject<UMaterial>();
	Glass->BaseColor = FLinearColor(0.2f, 0.5f, 0.9f, 1.0f);
	Glass->Opacity = 0.5f;
	const auto SpawnCube = [&World, Cube](const FVector& Location, const FVector& Scale, UMaterial* Material)
	{
		AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(Location, FRotator(0.0f, 30.0f, 0.0f));
		Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		(void)Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Actor->GetStaticMeshComponent()->SetMaterial(0, Material);
		Actor->SetActorScale3D(Scale);
		return Actor->GetStaticMeshComponent();
	};
	UStaticMeshComponent* Floor = SpawnCube(FVector(0.0f, 0.0f, -100.0f), FVector(20.0f, 20.0f, 0.2f), Textured);
	SpawnCube(FVector(200.0f, -120.0f, 0.0f), FVector(1.0f), Textured);
	SpawnCube(FVector(300.0f, 150.0f, 20.0f), FVector(1.5f), Flat);
	SpawnCube(FVector(100.0f, 20.0f, 0.0f), FVector(0.8f), Glass);
	// The floor's baked light: a gradient over its vertices, as a bake would give it.
	Floor->SetMobility(EComponentMobility::Static);
	FLPS2ColorStreams FloorColors;
	FloorColors.Init(Cube->GetLODResources().RenderData);
	for (int32 Offset = 0; Offset + 3 < FloorColors.Data.Num(); Offset += 4)
	{
		FloorColors.Data[Offset] = uint8(96 + ((Offset * 7) % 160));
		FloorColors.Data[Offset + 1] = uint8(80 + ((Offset * 11) % 170));
		FloorColors.Data[Offset + 2] = uint8(128 + ((Offset * 5) % 120));
	}
	Floor->SetBakedVertexColors(MoveTemp(FloorColors));
	(void)World.SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-50.0f, 40.0f, 0.0f));
	(void)World.SpawnActor<APointLight>(FVector(150.0f, 0.0f, 80.0f), FRotator(0.0f, 0.0f, 0.0f));
	// The scaled transforms reach the proxies before the frame, as in the engine's frame.
	World.SendAllEndOfFrameUpdates();

	UCameraComponent& Camera = *NewObject<UCameraComponent>();
	Camera.SetPerspective(
		60.0f, float(FGSOpenGLEmulator::FrameWidth) / float(FGSOpenGLEmulator::FrameHeight), 10.0f, 10000.0f);
	Camera.SetMode(ECameraMode::FreeLook);
	Camera.SetEyeLocation(FVector(-250.0f, 0.0f, 60.0f));
	Camera.SetViewRotation(FRotator(-15.0f, 5.0f, 0.0f));
	FSceneViewFamily Family(FSceneViewFamily::ConstructionValues(
		FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight, World.Scene, FEngineShowFlags()));
	const FSceneView View(FSceneView::FromCamera(Family, Camera));
	Family.Views.Add(&View);

	const FGSDrawEnvironment Environment = FGSOpenGLEmulator::GetDrawEnvironment();
	uint32 ArenaFirstBlock = 0;
	uint32 ArenaBlocks = 0;
	FGSOpenGLEmulator::GetTextureArena(ArenaFirstBlock, ArenaBlocks);
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	Renderer.GetTextureCache().SetTextureConverter(&ConvertTextureAsPS2Cook);
	FGSCommandList List;
	Environment.Append(List);
	Renderer.Render(Family, Environment, List);
	TestTrue("The frame draws triangles", Renderer.GetFrameStats().TrianglesSubmitted > 12);
	TestTrue("The texture was uploaded", Renderer.GetFrameStats().TextureUploads > 0);
	bool bTrilinear = false;
	for (const FGSRegisterWrite& Write : List.GetWrites())
	{
		const bool bTex1 = Write.Register == EGSRegister::TEX1_1;
		bTrilinear |= bTex1 && FGSTex1::Decode(Write.Value).MMIN == EGSFilter::LinearMipmapLinear &&
			FGSTex1::Decode(Write.Value).MXL > 0;
	}
	TestTrue("The texture is sampled through its mips", bTrilinear);

	Emulator.Execute(List);
	const TArray<FColor> Emulated = Emulator.ReadFrame(FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight);
	FGSReferenceRasterizer Reference;
	Reference.Execute(List);
	const TArray<FColor> Expected =
		Reference.ReadFrame(Environment.Frame, FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight);
	const int32 NumDifferent = CountDifferentPixels(
		TEXT("Scene frame"), Emulated, Expected, FGSOpenGLEmulator::FrameWidth, SceneChannelTolerance);
	TestTrue("The scene frame within the tolerance", NumDifferent <= SceneMaxDifferentPixels);
	Emulator.Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSEmulatorCanvasFrameTest, "System.Renderer.GSEmulator.CanvasFrame",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::SmokeFilter)

bool FGSEmulatorCanvasFrameTest::RunTest(const FString& Parameters)
{
	// The canvas's text (the four fonts, Spanish, a shadow, an outline) and textured tiles (one to one, scaled,
	// rotated) in the desktop's environment: the emulator draws what the reference draws from the same list.
	FScopedGLContext Context;
	if (!Context.IsValid())
	{
		AddError("No OpenGL context (a display is needed; run LeonAutomationTests with -nodisplay to skip this test)");
		return false;
	}
	FGSOpenGLEmulator Emulator;
	if (!TestTrue("The emulator starts", Emulator.Initialize(FPaths::Combine(FPaths::EngineDir(), TEXT("Shaders")))))
	{
		return false;
	}
	UTexture2D* Texture = CanvasTestScene::MakeTexture();
	FCanvas Canvas(FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight);
	CanvasTestScene::Draw(Canvas, Texture);
	const FGSDrawEnvironment Environment = FGSOpenGLEmulator::GetDrawEnvironment();
	uint32 ArenaFirstBlock = 0;
	uint32 ArenaBlocks = 0;
	FGSOpenGLEmulator::GetTextureArena(ArenaFirstBlock, ArenaBlocks);
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	Renderer.GetTextureCache().SetTextureConverter(&ConvertTextureAsPS2Cook);
	Renderer.GetTextureCache().BeginFrame();
	FGSCommandList List;
	Environment.Append(List);
	Renderer.DrawCanvas(Canvas, Environment, List);

	Emulator.Execute(List);
	const TArray<FColor> Emulated = Emulator.ReadFrame(FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight);
	FGSReferenceRasterizer Reference;
	Reference.Execute(List);
	const TArray<FColor> Expected =
		Reference.ReadFrame(Environment.Frame, FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight);
	const int32 NumDifferent = CountDifferentPixels(
		TEXT("Canvas frame"), Emulated, Expected, FGSOpenGLEmulator::FrameWidth, SceneChannelTolerance);
	TestTrue("The canvas frame within the tolerance", NumDifferent <= SceneMaxDifferentPixels);
	Emulator.Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSEmulatorRadarFrameTest, "System.Renderer.GSEmulator.RadarFrame",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::SmokeFilter)

bool FGSEmulatorRadarFrameTest::RunTest(const FString& Parameters)
{
	// ps2-polish P7: a game's radar as the canvas draws it: a dark square with its frame, a map's overview (a PSMT8
	// 128 x 128 texture, as LeonEd makes them) turned inside it as UV triangles clipped to the texture's edge (an
	// FCanvasTriangleItem), dots on top and a letter. The emulator draws what the reference draws from the same list,
	// and the overview's texels land in the square, not outside it.
	FScopedGLContext Context;
	if (!Context.IsValid())
	{
		AddError("No OpenGL context (a display is needed; run LeonAutomationTests with -nodisplay to skip this test)");
		return false;
	}
	FGSOpenGLEmulator Emulator;
	if (!TestTrue("The emulator starts", Emulator.Initialize(FPaths::Combine(FPaths::EngineDir(), TEXT("Shaders")))))
	{
		return false;
	}
	// The overview: pale floors crossed by dark walls, paletted as LeonEd paletizes it (PF_P8, one level).
	constexpr int32 Side = 128;
	TArray<uint8> Rgba;
	for (int32 Y = 0; Y < Side; ++Y)
	{
		for (int32 X = 0; X < Side; ++X)
		{
			const bool bWall = (X % 32) < 3 || (Y % 24) < 3;
			Rgba.Add(bWall ? 30 : uint8(150 + ((X * 3) % 80)));
			Rgba.Add(bWall ? 34 : uint8(130 + ((Y * 5) % 90)));
			Rgba.Add(bWall ? 28 : uint8(100 + (((X + Y) * 7) % 60)));
			Rgba.Add(255);
		}
	}
	FPalettedTexture Paletted;
	if (!TestTrue("Paletted", FPalettedTextureBuilder::Build(Rgba.GetData(), Side, Side, true, Paletted)) ||
		!TestEqual("PSMT8", int32(Paletted.Format), int32(PF_P8)))
	{
		return false;
	}
	UTexture2D* Overview = NewObject<UTexture2D>();
	if (!TestTrue("The overview's texels", Overview->SetPlatformData(Side, Side, PF_P8, Paletted.Data.GetData())))
	{
		return false;
	}

	// The radar: 96 pixels at (12, 12), the overview turned 30 degrees, 0.3 of it across, centred near its northern
	// edge (V 0.1): the square's polygon clipped at V = 0 as the HUD clips it, then fanned.
	FCanvas Canvas(FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight);
	Canvas.DrawTile(0.0f, 0.0f, 640.0f, 448.0f, FLinearColor(0.3f, 0.45f, 0.6f));
	Canvas.DrawTile(11.0f, 11.0f, 98.0f, 98.0f, FLinearColor(0.45f, 0.45f, 0.45f));
	Canvas.DrawTile(12.0f, 12.0f, 96.0f, 96.0f, FLinearColor(0.04f, 0.07f, 0.04f));
	const float Cos = FMath::Cos(FMath::DegreesToRadians(30.0f));
	const float Sin = FMath::Sin(FMath::DegreesToRadians(30.0f));
	auto UVOf = [Cos, Sin](const FVector2D& Screen)
	{
		const float Dx = (Screen.X - 60.0f) / 96.0f * 0.3f;
		const float Dy = (Screen.Y - 60.0f) / 96.0f * 0.3f;
		return FVector2D(0.5f + (Dx * Cos) - (Dy * Sin), 0.1f + (Dx * Sin) + (Dy * Cos));
	};
	const FVector2D Corners[] = {
		FVector2D(12.0f, 12.0f), FVector2D(108.0f, 12.0f), FVector2D(108.0f, 108.0f), FVector2D(12.0f, 108.0f)};
	TArray<FVector2D> Kept;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector2D& A = Corners[Index];
		const FVector2D& B = Corners[(Index + 1) % 4];
		const float VA = UVOf(A).Y;
		const float VB = UVOf(B).Y;
		if (VA >= 0.0f)
		{
			Kept.Add(A);
		}
		if ((VA >= 0.0f) != (VB >= 0.0f))
		{
			Kept.Add(A + ((B - A) * (-VA / (VB - VA))));
		}
	}
	TArray<FCanvasUVTri> Triangles;
	for (int32 Index = 1; Index + 1 < Kept.Num(); ++Index)
	{
		FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
		Triangle.V0_Pos = Kept[0];
		Triangle.V1_Pos = Kept[Index];
		Triangle.V2_Pos = Kept[Index + 1];
		Triangle.V0_UV = UVOf(Kept[0]).ComponentMax(FVector2D(0.0f, 0.0f));
		Triangle.V1_UV = UVOf(Kept[Index]).ComponentMax(FVector2D(0.0f, 0.0f));
		Triangle.V2_UV = UVOf(Kept[Index + 1]).ComponentMax(FVector2D(0.0f, 0.0f));
	}
	TestTrue("Clipped: more than a quad's two triangles", Triangles.Num() > 2);
	FCanvasTriangleItem Map(TArrayView<const FCanvasUVTri>(Triangles.GetData(), Triangles.Num()), Overview);
	Canvas.DrawItem(Map);
	Canvas.DrawTile(40.0f, 70.0f, 4.0f, 4.0f, FLinearColor(0.43f, 0.63f, 1.0f));
	Canvas.DrawTile(80.0f, 30.0f, 5.0f, 5.0f, FLinearColor(1.0f, 0.25f, 0.2f));
	Canvas.DrawTile(58.5f, 58.5f, 3.0f, 3.0f, FLinearColor::White);
	Canvas.DrawLine(60.0f, 60.0f, 51.5f, 51.5f, FLinearColor(0.3f, 0.45f, 0.3f), 1.0f);
	Canvas.DrawText(UEngine::GetSmallFont(), TEXT("A"), 90.0f, 80.0f, FLinearColor(0.85f, 0.85f, 0.85f));

	const FGSDrawEnvironment Environment = FGSOpenGLEmulator::GetDrawEnvironment();
	uint32 ArenaFirstBlock = 0;
	uint32 ArenaBlocks = 0;
	FGSOpenGLEmulator::GetTextureArena(ArenaFirstBlock, ArenaBlocks);
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	Renderer.GetTextureCache().SetTextureConverter(&ConvertTextureAsPS2Cook);
	Renderer.GetTextureCache().BeginFrame();
	FGSCommandList List;
	Environment.Append(List);
	Renderer.DrawCanvas(Canvas, Environment, List);

	Emulator.Execute(List);
	const TArray<FColor> Emulated = Emulator.ReadFrame(FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight);
	FGSReferenceRasterizer Reference;
	Reference.Execute(List);
	const TArray<FColor> Expected =
		Reference.ReadFrame(Environment.Frame, FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight);
	const int32 NumDifferent = CountDifferentPixels(
		TEXT("Radar frame"), Emulated, Expected, FGSOpenGLEmulator::FrameWidth, SceneChannelTolerance);
	TestTrue("The radar frame within the tolerance", NumDifferent <= SceneMaxDifferentPixels);
	// The overview's pale floors inside the square (not only its dark background), and outside it the backdrop.
	int32 Floors = 0;
	for (int32 Y = 14; Y < 106; ++Y)
	{
		for (int32 X = 14; X < 106; ++X)
		{
			Floors += Expected[(Y * FGSOpenGLEmulator::FrameWidth) + X].R > 90 ? 1 : 0;
		}
	}
	TestTrue(*FString::Printf("The overview shows in the square (%d floor pixels)", Floors), Floors > 1000);
	const FColor& Outside = Expected[(200 * FGSOpenGLEmulator::FrameWidth) + 300];
	TestTrue("Nothing of it outside", Outside.B > Outside.R);
	Emulator.Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSEmulatorDebugTextTest, "System.Renderer.GSEmulator.DebugText",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::SmokeFilter)

bool FGSEmulatorDebugTextTest::RunTest(const FString& Parameters)
{
	// FGSDebugDraw's text as the PS2's error screen records it (Docs/PLANS/ps2-polish.md P5b): the compiled-in font
	// uploaded to the texture arena, both sizes over a flat backdrop, on the desktop's 16-bit dithered frame (the error
	// screen's): the emulator draws what the reference draws from the same list.
	FScopedGLContext Context;
	if (!Context.IsValid())
	{
		AddError("No OpenGL context (a display is needed; run LeonAutomationTests with -nodisplay to skip this test)");
		return false;
	}
	FGSOpenGLEmulator Emulator;
	if (!TestTrue("The emulator starts", Emulator.Initialize(FPaths::Combine(FPaths::EngineDir(), TEXT("Shaders")))))
	{
		return false;
	}
	const FGSDrawEnvironment Environment = FGSOpenGLEmulator::GetDrawEnvironment();
	uint32 FontBlock = 0;
	uint32 ArenaBlocks = 0;
	FGSOpenGLEmulator::GetTextureArena(FontBlock, ArenaBlocks);
	FGSCommandList List;
	Environment.Append(List);
	FGSDebugDraw::UploadFont(List, FontBlock);
	FGSDebugDraw::DrawRect(List, Environment, 0.0f, 0.0f, float(FGSOpenGLEmulator::FrameWidth),
		float(FGSOpenGLEmulator::FrameHeight), FGSDebugDraw::UnitColor(0.25f, 0.02f, 0.02f));
	FGSDebugDraw::DrawString(List, Environment, FontBlock, 24.0f, 24.0f, "The game stopped (exit code 1)",
		FGSDebugDraw::UnitColor(1.0f, 0.9f, 0.6f));
	FGSDebugDraw::DrawString(List, Environment, FontBlock, 24.0f, 52.0f,
		"LogPakFile: Error: cannot open 'host:ShooterGame/Content/Paks/ShooterGame-PS2.lpak' (A\xc3\xb1o, "
		"\xc2\xbfQu\xc3\xa9?)",
		FGSDebugDraw::UnitColor(1.0f, 0.75f, 0.75f), EGSDebugFont::Tiny);

	Emulator.Execute(List);
	const TArray<FColor> Emulated = Emulator.ReadFrame(FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight);
	FGSReferenceRasterizer Reference;
	Reference.Execute(List);
	const TArray<FColor> Expected =
		Reference.ReadFrame(Environment.Frame, FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight);
	const int32 NumDifferent = CountDifferentPixels(
		TEXT("Debug text frame"), Emulated, Expected, FGSOpenGLEmulator::FrameWidth, SceneChannelTolerance);
	TestTrue("The debug text within the tolerance", NumDifferent <= SceneMaxDifferentPixels);
	Emulator.Shutdown();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
