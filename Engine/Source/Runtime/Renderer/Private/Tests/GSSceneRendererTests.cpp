#include "Camera/CameraComponent.h"
#include "CanvasTypes.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GS/GSPrimitiveEmitter.h"
#include "GS/GSSceneRenderer.h"
#include "GS/GSTextureCache.h"
#include "GSReferenceRasterizer.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Primitives.h"
#include "SceneView.h"
#include "StaticMeshSceneProxy.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// The GS scene renderer (Docs/PLANS/ps2-gs-parity.md P5): the emitter's clipping and mapping, the texture cache, and
// frames recorded by FGSSceneRenderer and drawn by the reference rasterizer, the GS's oracle.

namespace
{

	constexpr int32 FrameWidth = 640;
	constexpr int32 FrameHeight = 448;

	/** A 640 x 448 PSMCT32 frame at FBP 0, its PSMZ24 Z buffer after it, as the PS2 lays out one frame. */
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

	/** The texture arena after the frame and the Z buffer (pages 280 to 511). */
	constexpr uint32 ArenaFirstBlock = 280 * 32;
	constexpr uint32 ArenaBlocks = (512 - 280) * 32;

	TArray<FColor> Rasterize(const FGSCommandList& List, const FGSDrawEnvironment& Environment)
	{
		FGSReferenceRasterizer Rasterizer;
		Rasterizer.Execute(List);
		return Rasterizer.ReadFrame(Environment.Frame, FrameWidth, FrameHeight);
	}

	const FColor& PixelAt(const TArray<FColor>& Pixels, int32 X, int32 Y)
	{
		return Pixels[(Y * FrameWidth) + X];
	}

	/** The vertices the list kicks, in order. */
	TArray<FGSXYZ> KickedVertices(const FGSCommandList& List)
	{
		TArray<FGSXYZ> Vertices;
		for (const FGSRegisterWrite& Write : List.GetWrites())
		{
			if (Write.Register == EGSRegister::XYZ2)
			{
				Vertices.Add(FGSXYZ::Decode(Write.Value));
			}
		}
		return Vertices;
	}

	FGSClipVertex ClipVertex(float X, float Y, float Z = 0.5f, float W = 1.0f)
	{
		FGSClipVertex Vertex;
		Vertex.Clip = FVector4(X, Y, Z, W);
		return Vertex;
	}

	/** A camera at (-Distance, 0, 0) looking down +X, 60 degrees vertically, near 10 cm. */
	UCameraComponent& MakeCamera(float Distance)
	{
		UCameraComponent& Camera = *NewObject<UCameraComponent>();
		Camera.SetPerspective(60.0f, float(FrameWidth) / float(FrameHeight), 10.0f, 10000.0f);
		Camera.SetMode(ECameraMode::Orbit);
		Camera.SetTarget(FVector::ZeroVector);
		Camera.SetViewRotation(FRotator(0.0f, 0.0f, 0.0f));
		Camera.SetDistance(Distance);
		return Camera;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSEmitterMappingTest, "System.Renderer.GS.Emitter.MappingAndCulling",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSEmitterMappingTest::RunTest(const FString& Parameters)
{
	// NDC (0, 0) is the frame's center, on the pixel corner OpenGL samples around (GS pixel centers are integers, so
	// half a pixel less); depth 0 (the near plane) is the largest Z. Counter-clockwise triangles (Y up) are front
	// faces; a triangle wholly outside is rejected.
	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSCommandList List;
	FGSPrimitiveEmitter Emitter(Environment, List);
	Emitter.BeginTriangles(false, false, true);
	Emitter.AddTriangle(ClipVertex(0.0f, 0.0f, 0.0f), ClipVertex(0.5f, 0.0f), ClipVertex(0.0f, 0.5f));
	const TArray<FGSXYZ> Front = KickedVertices(List);
	TestEqual("Three vertices", Front.Num(), 3);
	if (Front.Num() != 3)
	{
		return false;
	}
	TestEqual("Center X", int32(Front[0].X), int32(GSToFixed4(Environment.PrimitiveX(319.5f), 16)));
	TestEqual("Center Y", int32(Front[0].Y), int32(GSToFixed4(Environment.PrimitiveY(223.5f), 16)));
	TestEqual("Near plane: the largest Z", Front[0].Z, FGSDrawEnvironment::MaxDepth24);
	TestTrue("NDC +Y is up on screen", Front[2].Y < Front[0].Y);

	Emitter.AddTriangle(ClipVertex(0.0f, 0.0f), ClipVertex(0.0f, 0.5f), ClipVertex(0.5f, 0.0f));
	TestEqual("A clockwise triangle is culled", KickedVertices(List).Num(), 3);
	Emitter.AddTriangle(ClipVertex(2.0f, 0.0f), ClipVertex(3.0f, 0.0f), ClipVertex(2.0f, 0.5f));
	TestEqual("Outside the view: rejected", KickedVertices(List).Num(), 3);
	TestEqual("Two rejected", Emitter.GetNumRejected(), 2);

	// One vertex behind the near plane: the triangle becomes a quad (two triangles) with no vertex behind it.
	FGSCommandList Clipped;
	FGSPrimitiveEmitter NearEmitter(Environment, Clipped);
	NearEmitter.BeginTriangles(false, false, false);
	NearEmitter.AddTriangle(
		ClipVertex(-0.5f, -0.5f, 0.5f), ClipVertex(0.5f, -0.5f, 0.5f), ClipVertex(0.0f, 0.5f, -0.5f));
	TestEqual("Clipped into two triangles", NearEmitter.GetNumTriangles(), 2);
	bool bAllInFront = true;
	for (const FGSXYZ& Vertex : KickedVertices(Clipped))
	{
		bAllInFront &= Vertex.Z <= FGSDrawEnvironment::MaxDepth24;
	}
	TestTrue("Z in range", bAllInFront);

	// Far off to the side but in front: clipped to the guard band, within the GS's primitive space.
	FGSCommandList Guard;
	FGSPrimitiveEmitter GuardEmitter(Environment, Guard);
	GuardEmitter.BeginTriangles(false, false, false);
	GuardEmitter.AddTriangle(ClipVertex(-0.5f, -0.5f), ClipVertex(100.0f, -0.5f), ClipVertex(-0.5f, 0.5f));
	bool bInSpace = KickedVertices(Guard).Num() >= 3;
	for (const FGSXYZ& Vertex : KickedVertices(Guard))
	{
		bInSpace &= Vertex.X < GSToFixed4(4095.0f, 16);
	}
	TestTrue("Guard band keeps X in 0..4095", bInSpace);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTextureCacheTest, "System.Renderer.GS.TextureCache",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSTextureCacheTest::RunTest(const FString& Parameters)
{
	// A texture uploads the first time it is bound (a power of two at least 8 texels a side, in whole pages) and not
	// again; an arena too small for the next texture starts over.
	FGSTextureCache Cache;
	FGSCommandList List;
	FGSTex0 Tex0;
	TArray<uint8> Texels;
	Texels.Init(200, 5 * 3 * 4);
	int32 KeyA = 0;
	int32 KeyB = 0;
	TestFalse("No arena, no texture", Cache.BindTexels(&KeyA, 5, 3, Texels, false, List, Tex0));
	Cache.SetArena(ArenaFirstBlock, 64);
	TestTrue("Bound", Cache.BindTexels(&KeyA, 5, 3, Texels, false, List, Tex0));
	TestEqual("Uploaded once", Cache.GetNumUploads(), 1);
	TestEqual("8 texels wide", int32(Tex0.TW), 3);
	TestEqual("8 texels high", int32(Tex0.TH), 3);
	TestEqual("At the arena", int32(Tex0.TBP0), int32(ArenaFirstBlock));
	TestTrue("Bound again", Cache.BindTexels(&KeyA, 5, 3, Texels, false, List, Tex0));
	TestEqual("Not uploaded again", Cache.GetNumUploads(), 1);
	TestTrue("A second one", Cache.BindTexels(&KeyB, 5, 3, Texels, false, List, Tex0));
	TestEqual("The next page", int32(Tex0.TBP0), int32(ArenaFirstBlock + 32));
	int32 KeyC = 0;
	TestTrue("A third starts over", Cache.BindTexels(&KeyC, 5, 3, Texels, false, List, Tex0));
	TestEqual("At the arena again", int32(Tex0.TBP0), int32(ArenaFirstBlock));
	TestEqual("One resident", Cache.GetNumResident(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneRendererLitCubeTest, "System.Renderer.GS.Scene.LitCube",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneRendererLitCubeTest::RunTest(const FString& Parameters)
{
	// A cube at the origin seen from 3 m down +X, lit head-on by a white sun travelling +X: its face is the albedo
	// times 1.1 (0.10 ambient + N.L) with an untextured material, the corners are the clear colour, and the face wrote
	// its depth.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	UStaticMesh* Cube = NewObject<UStaticMesh>();
	(void)Cube->BuildFromMeshData(MakeCube());
	AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator(0.0f, 0.0f, 0.0f));
	(void)Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
	UMaterial* Flat = NewObject<UMaterial>();
	Flat->BaseColor = FLinearColor(0.5f, 0.25f, 0.75f, 1.0f);
	Actor->GetStaticMeshComponent()->SetMaterial(0, Flat);
	(void)World.SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(0.0f, 0.0f, 0.0f));

	UCameraComponent& Camera = MakeCamera(300.0f);
	FSceneViewFamily Family(
		FSceneViewFamily::ConstructionValues(FrameWidth, FrameHeight, World.Scene, FEngineShowFlags()));
	const FSceneView View(FSceneView::FromCamera(Family, Camera));
	Family.Views.Add(&View);

	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSCommandList List;
	Environment.Append(List);
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	Renderer.Render(Family, Environment, List);
	FGSReferenceRasterizer Rasterizer;
	Rasterizer.Execute(List);
	const TArray<FColor> Pixels = Rasterizer.ReadFrame(Environment.Frame, FrameWidth, FrameHeight);

	const FStaticMeshSceneProxy* Proxy =
		static_cast<const FStaticMeshSceneProxy*>(Actor->GetStaticMeshComponent()->SceneProxy);
	if (!TestNotNull("A proxy", Proxy))
	{
		return false;
	}
	const FVector Albedo = Proxy->GetSectionMaterial(0).Albedo;
	const auto Expected = [](float Value) { return FMath::Clamp(FMath::RoundToInt(Value * 1.1f * 255.0f), 0, 255); };
	const FColor& Center = PixelAt(Pixels, FrameWidth / 2, FrameHeight / 2);

	TestTrue("Lit face, red", FMath::Abs(int32(Center.R) - Expected(Albedo.X)) <= 1);
	TestTrue("Lit face, green", FMath::Abs(int32(Center.G) - Expected(Albedo.Y)) <= 1);
	TestTrue("Lit face, blue", FMath::Abs(int32(Center.B) - Expected(Albedo.Z)) <= 1);
	const FColor& Corner = PixelAt(Pixels, 2, 2);
	TestEqual("Clear colour", int32(Corner.R), FMath::RoundToInt(FGSSceneRenderer::ClearColor.R * 255.0f));
	TestTrue("The face wrote its depth",
		Rasterizer.ReadZ(Environment.ZBuf, FrameWidth, FrameWidth / 2, FrameHeight / 2) > 0);
	TestEqual("Background Z", Rasterizer.ReadZ(Environment.ZBuf, FrameWidth, 2, 2), 0u);
	TestEqual("One object", Renderer.GetFrameStats().ObjectsVisible, 1);
	TestTrue("The near faces only (back faces culled)", Renderer.GetFrameStats().TrianglesSubmitted <= 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneRendererCanvasTest, "System.Renderer.GS.Canvas",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneRendererCanvasTest::RunTest(const FString& Parameters)
{
	// A tile covers [X, X + Size) x [Y, Y + Size) of the frame in its colour, over whatever is there.
	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSCommandList List;
	Environment.Append(List);
	FCanvas Canvas(FrameWidth, FrameHeight);
	Canvas.DrawTile(10.0f, 20.0f, 30.0f, 5.0f, FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
	FGSSceneRenderer Renderer;
	Renderer.DrawCanvas(Canvas, Environment, List);
	const TArray<FColor> Pixels = Rasterize(List, Environment);
	TestEqual("Its first pixel", int32(PixelAt(Pixels, 10, 20).R), 255);
	TestEqual("Its last pixel", int32(PixelAt(Pixels, 39, 24).R), 255);
	TestEqual("Right of it", int32(PixelAt(Pixels, 40, 22).R), 0);
	TestEqual("Below it", int32(PixelAt(Pixels, 20, 25).R), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
