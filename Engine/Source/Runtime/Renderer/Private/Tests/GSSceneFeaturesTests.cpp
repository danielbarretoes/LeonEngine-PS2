#include "Camera/CameraComponent.h"
#include "CanvasTypes.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/VisibilityCellVolume.h"
#include "Engine/VisibilityPortal.h"
#include "Engine/World.h"
#include "GS/GSSceneRenderer.h"
#include "GSPrimitiveEmitter.h"
#include "GSReferenceRasterizer.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Primitives.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "StaticMeshSceneProxy.h"
#include "Tests/ScopedTestWorld.h"
#include "WorldEffectsGeometry.h"

#if WITH_DEV_AUTOMATION_TESTS

// The scene features of Docs/PLANS/ps2-shipping.md N15, drawn by the GS scene renderer and checked on the reference
// rasterizer: static mesh LODs by screen size, the distance fog, blob shadows, the canvas's sprites, cells and portals,
// and the view model pass's culling.

namespace
{

	constexpr int32 FrameWidth = 640;
	constexpr int32 FrameHeight = 448;
	constexpr uint32 ArenaFirstBlock = 280 * 32;
	constexpr uint32 ArenaBlocks = (512 - 280) * 32;

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

	TArray<FColor> Rasterize(const FGSCommandList& List, const FGSDrawEnvironment& Environment)
	{
		FGSReferenceRasterizer Rasterizer;
		Rasterizer.Execute(List);
		return Rasterizer.ReadFrame(Environment.Frame, FrameWidth, FrameHeight);
	}

	/** A camera at Eye looking along Rotation, 60 degrees vertically, near 10 cm. */
	UCameraComponent& MakeCamera(const FVector& Eye, const FRotator& Rotation)
	{
		UCameraComponent& Camera = *NewObject<UCameraComponent>();
		Camera.SetPerspective(60.0f, float(FrameWidth) / float(FrameHeight), 10.0f, 100000.0f);
		Camera.SetMode(ECameraMode::FreeLook);
		Camera.SetEyeLocation(Eye);
		Camera.SetViewRotation(Rotation);
		return Camera;
	}

	/** An unlit static mesh actor of Source at Location, scaled, in Color; Static, colliding when asked. */
	AStaticMeshActor* SpawnMesh(UWorld& World, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale,
		const FLinearColor& Color, bool bCollide = false)
	{
		AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(Location, FRotator(0.0f, 0.0f, 0.0f));
		UStaticMeshComponent& Component = *Actor->GetStaticMeshComponent();
		(void)Component.SetStaticMesh(Mesh);
		UMaterial* Material = NewObject<UMaterial>();
		Material->ShadingModel = MSM_Unlit;
		Material->BaseColor = Color;
		Component.SetMaterial(0, Material);
		Actor->SetActorScale3D(Scale);
		if (bCollide)
		{
			Component.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		return Actor;
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

	/** A sphere mesh with LODs: half its triangles below 0.3 of the view's height, a quarter below 0.1. */
	UStaticMesh* MakeSphereWithLODs()
	{
		UStaticMesh* Mesh = NewObject<UStaticMesh>();
		Mesh->SourceModels.SetNum(3);
		Mesh->SourceModels[1].ReductionSettings.PercentTriangles = 0.5f;
		Mesh->SourceModels[1].ScreenSize = 0.3f;
		Mesh->SourceModels[2].ReductionSettings.PercentTriangles = 0.25f;
		Mesh->SourceModels[2].ScreenSize = 0.1f;
		(void)Mesh->BuildFromMeshData(MakeSphere(24, 16));
		return Mesh;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneLODByScreenSizeTest, "System.Renderer.GS.Scene.LODByScreenSize",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneLODByScreenSizeTest::RunTest(const FString& Parameters)
{
	// A 1 m sphere with two simplified LODs (meshoptimizer, built with the mesh): its bounds' sphere (86.6 cm) through
	// a 60 degree projection is 150 / D of the view's height at D cm, so LOD 0 down to 5 m, LOD 1 to 15 m, LOD 2
	// beyond; the distance scale moves the thresholds; the renderer draws the far one with fewer triangles.
	UStaticMesh* Mesh = MakeSphereWithLODs();
	if (!TestEqual("Three LODs", Mesh->GetNumLODs(), 3))
	{
		return false;
	}
	const int32 Triangles0 = Mesh->GetLODResources(0).GetNumTriangles();
	const int32 Triangles1 = Mesh->GetLODResources(1).GetNumTriangles();
	const int32 Triangles2 = Mesh->GetLODResources(2).GetNumTriangles();
	TestTrue("Fewer triangles each LOD", Triangles1 < Triangles0 && Triangles2 < Triangles1);
	TestEqual(
		"The sections kept", Mesh->GetLODResources(2).GetNumSections(), Mesh->GetLODResources(0).GetNumSections());

	const float HalfFov = FMath::DegreesToRadians(30.0f);
	const FPerspectiveMatrix Projection(HalfFov, HalfFov, 1.0f, 1.0f, 10.0f, 100000.0f);
	const float Radius = Mesh->GetBoundingBox().GetExtent().Size();
	const auto LODAt = [&](float Distance, float Scale)
	{
		return ComputeStaticMeshLOD(
			*Mesh, FVector(Distance, 0.0f, 0.0f), Radius, FVector::ZeroVector, Projection, Scale);
	};
	TestEqual("3 m: LOD 0", LODAt(300.0f, 1.0f), 0);
	TestEqual("10 m: LOD 1", LODAt(1000.0f, 1.0f), 1);
	TestEqual("30 m: LOD 2", LODAt(3000.0f, 1.0f), 2);
	TestEqual("Just nearer than the threshold (0.3 at 5 m): LOD 0", LODAt(490.0f, 1.0f), 0);
	TestEqual("Just past it: LOD 1", LODAt(510.0f, 1.0f), 1);
	TestEqual("3 m with the distance scale 2: LOD 1", LODAt(300.0f, 2.0f), 1);
	TestEqual("30 m with the distance scale 0.1: LOD 0", LODAt(3000.0f, 0.1f), 0);
	TestEqual("The screen size at 10 m",
		ComputeBoundsScreenSize(FVector(1000.0f, 0.0f, 0.0f), Radius, FVector::ZeroVector, Projection),
		2.0f * 0.5f * Projection.M[1][1] * Radius / 1000.0f, 1.0e-4f);

	// Drawn 30 m away: LOD 2's triangles; with a distance scale that keeps LOD 0, all of them.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AStaticMeshActor* Actor = SpawnMesh(World, Mesh, FVector(3000.0f, 0.0f, 0.0f), FVector(1.0f), FLinearColor::White);
	Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	UCameraComponent& Camera = MakeCamera(FVector::ZeroVector, FRotator(0.0f, 0.0f, 0.0f));
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	FGSCommandList List;
	const FFrameStats Far = RenderFrame(World, Camera, Renderer, List);
	Renderer.SetLODDistanceScale(0.01f);
	const FFrameStats Near = RenderFrame(World, Camera, Renderer, List);
	TestEqual("At a lower LOD", Far.ObjectsAtLowerLOD, 1);
	TestEqual("At LOD 0 with the scale", Near.ObjectsAtLowerLOD, 0);
	TestTrue("Fewer triangles drawn", Far.TrianglesSubmitted > 0 && Far.TrianglesSubmitted < Near.TrianglesSubmitted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneFogTest, "System.Renderer.GS.Scene.Fog",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneFogTest::RunTest(const FString& Parameters)
{
	// The world settings' linear fog from 2 m to 12 m: F is 255 at the start, 0 at the end, a line between; a white
	// cube 1.5 m away keeps its colour, one 30 m away is the fog's colour; the vertices are XYZF2 with the F of their
	// depth; the vertex batch commands (VU1's) expanded by the C++ emitter draw the same frame.
	const FGSVertexFog Line = FGSVertexFog::MakeLinear(200.0f, 1200.0f);
	TestEqual("F at the start", int32(Line.GetF(200.0f)), 255);
	TestEqual("F at the end", int32(Line.GetF(1200.0f)), 0);
	TestEqual("F halfway", int32(Line.GetF(700.0f)), 128);
	TestEqual("F before the start", int32(Line.GetF(10.0f)), 255);
	TestEqual("F past the end", int32(Line.GetF(5000.0f)), 0);

	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	// A map's world settings (the test world has none until one is set).
	AWorldSettings* WorldSettings = World.SpawnActor<AWorldSettings>();
	World.PersistentLevel->SetWorldSettings(WorldSettings);
	if (!TestTrue("World settings", WorldSettings != nullptr && World.GetWorldSettings() == WorldSettings))
	{
		return false;
	}
	WorldSettings->FogSettings.bEnableFog = true;
	WorldSettings->FogSettings.StartDistance = 200.0f;
	WorldSettings->FogSettings.EndDistance = 1200.0f;
	WorldSettings->FogSettings.FogInscatteringColor = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
	UStaticMesh* Cube = NewObject<UStaticMesh>();
	(void)Cube->BuildFromMeshData(MakeCube());
	(void)SpawnMesh(World, Cube, FVector(200.0f, -60.0f, 0.0f), FVector(0.5f), FLinearColor::White);
	(void)SpawnMesh(World, Cube, FVector(3000.0f, 600.0f, 0.0f), FVector(5.0f), FLinearColor::White);
	UCameraComponent& Camera = MakeCamera(FVector::ZeroVector, FRotator(0.0f, 0.0f, 0.0f));
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	FGSCommandList Emitted;
	(void)RenderFrame(World, Camera, Renderer, Emitted);
	int32 NumFogged = 0;
	uint32 MinF = 255;
	uint32 MaxF = 0;
	bool bFogCol = false;
	for (const FGSRegisterWrite& Write : Emitted.GetWrites())
	{
		if (Write.Register == EGSRegister::XYZF2 || Write.Register == EGSRegister::XYZF3)
		{
			const uint32 F = FGSXYZF::Decode(Write.Value).F;
			MinF = FMath::Min(MinF, F);
			MaxF = FMath::Max(MaxF, F);
			++NumFogged;
		}
		bFogCol |= Write.Register == EGSRegister::FOGCOL;
	}
	TestTrue("The fog's colour written", bFogCol);
	TestTrue("Fogged vertices", NumFogged > 0);
	TestTrue("Near and far F", MaxF > 200 && MinF == 0);
	const TArray<FColor> Pixels = Rasterize(Emitted, MakeEnvironment());
	// The near cube is left of the centre, the far one right of it.
	const FColor Near = Pixels[((FrameHeight / 2) * FrameWidth) + (FrameWidth / 2) - 130];
	const FColor Far = Pixels[((FrameHeight / 2) * FrameWidth) + (FrameWidth / 2) + 84];
	TestTrue("The near cube white", Near.R > 200 && Near.G > 200);
	TestTrue("The far cube the fog's blue", Far.R < 10 && Far.G < 10 && Far.B > 245);

	Renderer.SetVertexBatches(true);
	FGSCommandList Recorded;
	(void)RenderFrame(World, Camera, Renderer, Recorded);
	TestTrue("Fogged draws recorded", Recorded.GetVertexDraws().Num() > 0 && Recorded.GetVertexDraws()[0].Fog.bEnabled);
	FGSCommandList Expanded;
	Expanded.AppendExpanded(Recorded, MakeEnvironment());
	const TArray<FColor> Drawn = Rasterize(Expanded, MakeEnvironment());
	int32 NumDifferent = 0;
	for (int32 Index = 0; Index < Drawn.Num(); ++Index)
	{
		NumDifferent += Drawn[Index] == Pixels[Index] ? 0 : 1;
	}
	TestEqual("VU1's batches draw the same fog", NumDifferent, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneBlobShadowsTest, "System.Renderer.GS.Scene.BlobShadows",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneBlobShadowsTest::RunTest(const FString& Parameters)
{
	// Placement: under the bounds' centre on the floor's plane, 1.2 times their half width, fading with the height; no
	// shadow on a wall or far above the floor. Drawn: a cube casting one darkens the floor under it and nothing else.
	FWorldEffectsGeometry::FBlobShadow Shadow;
	const FBox Standing(FVector(-30.0f, -20.0f, 10.0f), FVector(30.0f, 20.0f, 190.0f));
	TestTrue("On the floor",
		FWorldEffectsGeometry::PlaceBlobShadow(Standing, FVector(5.0f, 5.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), Shadow));
	TestTrue("Under the centre", Shadow.Center.Equals(FVector(0.0f, 0.0f, 0.0f), 1.0e-3f));
	TestEqual("As wide as the bounds", Shadow.HalfSize, 30.0f * FWorldEffectsGeometry::BlobShadowSizeScale, 1.0e-3f);
	TestEqual("10 cm up", Shadow.Opacity,
		FWorldEffectsGeometry::BlobShadowOpacity * (1.0f - (10.0f / FWorldEffectsGeometry::BlobShadowFadeHeight)),
		1.0e-4f);
	TestTrue("On a ramp: under the centre along the vertical",
		FWorldEffectsGeometry::PlaceBlobShadow(
			Standing, FVector::ZeroVector, FVector(0.0f, 1.0f, 2.0f).GetSafeNormal(), Shadow) &&
			FMath::IsNearlyEqual(Shadow.Center.X, 0.0f) && FMath::IsNearlyEqual(Shadow.Center.Z, 0.0f, 1.0e-3f));
	TestFalse("Not on a wall",
		FWorldEffectsGeometry::PlaceBlobShadow(
			Standing, FVector::ZeroVector, FVector(1.0f, 0.0f, 0.2f).GetSafeNormal(), Shadow));
	TestFalse("Not from high above",
		FWorldEffectsGeometry::PlaceBlobShadow(
			Standing.ShiftBy(FVector(0.0f, 0.0f, 500.0f)), FVector::ZeroVector, FVector(0.0f, 0.0f, 1.0f), Shadow));

	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	UStaticMesh* Cube = NewObject<UStaticMesh>();
	(void)Cube->BuildFromMeshData(MakeCube());
	(void)SpawnMesh(World, Cube, FVector(0.0f, 0.0f, -50.0f), FVector(10.0f, 10.0f, 1.0f),
		FLinearColor(0.8f, 0.8f, 0.8f), /*bCollide =*/true);
	AStaticMeshActor* Caster =
		SpawnMesh(World, Cube, FVector(0.0f, 0.0f, 50.0f), FVector(0.5f), FLinearColor(1.0f, 0.2f, 0.2f));
	UStaticMeshComponent& CasterComponent = *Caster->GetStaticMeshComponent();
	CasterComponent.SetMobility(EComponentMobility::Movable);
	UCameraComponent& Camera = MakeCamera(FVector(-300.0f, 0.0f, 250.0f), FRotator(-40.0f, 0.0f, 0.0f));
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	FGSCommandList List;
	const FFrameStats Without = RenderFrame(World, Camera, Renderer, List);
	const TArray<FColor> Plain = Rasterize(List, MakeEnvironment());
	CasterComponent.SetCastBlobShadow(true);
	const FFrameStats With = RenderFrame(World, Camera, Renderer, List);
	const TArray<FColor> Shadowed = Rasterize(List, MakeEnvironment());
	FVector FloorPoint;
	FVector FloorNormal;
	TestTrue("The floor found under it",
		CasterComponent.SceneProxy != nullptr &&
			CasterComponent.SceneProxy->GetBlobShadowFloor(FloorPoint, FloorNormal) &&
			FMath::IsNearlyEqual(FloorPoint.Z, 0.0f, 0.5f) && FloorNormal.Z > 0.99f);
	TestEqual("No shadow without the flag", Without.BlobShadows, 0);
	TestEqual("One shadow with it", With.BlobShadows, 1);
	int32 NumDarker = 0;
	int32 NumBrighter = 0;
	for (int32 Index = 0; Index < Plain.Num(); ++Index)
	{
		NumDarker += Shadowed[Index].G < Plain[Index].G ? 1 : 0;
		NumBrighter += Shadowed[Index].G > Plain[Index].G ? 1 : 0;
	}
	UE_LOG(LogTemp, Display, "%s", *FString::Printf("Blob shadow: %d pixel(s) darker", NumDarker));
	TestTrue("The floor darker under the cube", NumDarker > 200);
	TestEqual("Nothing brighter", NumBrighter, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSCanvasSpritesTest, "System.Renderer.GS.Canvas.Sprites",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSCanvasSpritesTest::RunTest(const FString& Parameters)
{
	// The canvas's tiles, text and lines along an axis go to the GS as SPRITEs of two vertices (N15), its slanted lines
	// as triangles; the frame is the one two triangles a rectangle draw, pixel for pixel (SPRITE is StripsAndSprites'
	// conformance scene: nothing new asked of the GS), with far fewer vertices.
	FCanvas Canvas(FrameWidth, FrameHeight);
	Canvas.DrawTile(10.0f, 20.0f, 300.0f, 60.0f, FLinearColor(0.1f, 0.2f, 0.3f));
	Canvas.DrawTile(12.5f, 22.25f, 30.5f, 5.75f, FLinearColor(1.0f, 0.5f, 0.0f));
	Canvas.DrawText(TEXT("HP 100  $800  1:45"), 20.0f, 30.0f, FColor(255, 255, 0), 2.0f);
	Canvas.DrawLine(320.0f, 200.0f, 400.0f, 200.0f, FLinearColor::Green, 3.0f);
	Canvas.DrawLine(320.0f, 210.0f, 320.0f, 300.0f, FLinearColor::Green, 2.0f);
	Canvas.DrawLine(420.0f, 210.0f, 500.0f, 290.0f, FLinearColor::Red, 2.0f);
	Canvas.PushDepthSortKey(1);
	Canvas.DrawTextBlock(TEXT("stat unit\nFrame 33.5 ms"), 400.0f, 20.0f, FColor(200, 255, 200), 1.0f);
	Canvas.PopDepthSortKey();

	TArray<FCanvasVertex> Vertices;
	TArray<FCanvasPrimitiveRun> Runs;
	Canvas.GetPrimitives(Vertices, Runs);
	int32 NumRectangles = 0;
	int32 NumTriangles = 0;
	for (const FCanvasPrimitiveRun& Run : Runs)
	{
		(Run.Type == ECanvasPrimitive::Rectangle ? NumRectangles : NumTriangles) +=
			Run.NumVertices / (Run.Type == ECanvasPrimitive::Rectangle ? 2 : 3);
	}
	TestEqual("The slanted line's two triangles", NumTriangles, 2);
	TestTrue("Rectangles for the rest", NumRectangles > 20);

	const FGSDrawEnvironment Environment = MakeEnvironment();
	FGSSceneRenderer Renderer;
	FGSCommandList Sprites;
	Environment.Append(Sprites);
	Renderer.DrawCanvas(Canvas, Environment, Sprites);
	int32 NumSpritePrims = 0;
	for (const FGSRegisterWrite& Write : Sprites.GetWrites())
	{
		NumSpritePrims +=
			Write.Register == EGSRegister::PRIM && FGSPrim::Decode(Write.Value).Type == EGSPrimitive::Sprite;
	}
	TestTrue("SPRITE primitives", NumSpritePrims > 0);

	// The same items, every rectangle as two triangles (as the canvas drew them before).
	FGSCommandList Triangles;
	Environment.Append(Triangles);
	Triangles.SetTest(0, FGSDrawEnvironment::DepthTest(false));
	FGSPrim Prim;
	Prim.Type = EGSPrimitive::Triangle;
	Prim.bGouraud = true;
	Prim.bAlphaBlend = true;
	Triangles.SetPrim(Prim);
	int32 NumTriangleVertices = 0;
	const auto Emit = [&](float X, float Y, const FCanvasVertex& Color)
	{
		FGSRGBAQ Rgba;
		Rgba.R = uint8(FMath::Clamp(FMath::RoundToInt(Color.R * 255.0f), 0, 255));
		Rgba.G = uint8(FMath::Clamp(FMath::RoundToInt(Color.G * 255.0f), 0, 255));
		Rgba.B = uint8(FMath::Clamp(FMath::RoundToInt(Color.B * 255.0f), 0, 255));
		Rgba.A = uint8(FMath::Clamp(FMath::RoundToInt(Color.A * 128.0f), 0, 0x80));
		Triangles.SetRGBAQ(Rgba);
		Triangles.AddVertex(Environment.PixelVertex(X - 0.5f, Y - 0.5f, 0));
		++NumTriangleVertices;
	};
	for (const FCanvasPrimitiveRun& Run : Runs)
	{
		if (Run.Type == ECanvasPrimitive::Triangle)
		{
			for (int32 Index = Run.FirstVertex; Index < Run.FirstVertex + Run.NumVertices; ++Index)
			{
				Emit(Vertices[Index].X, Vertices[Index].Y, Vertices[Index]);
			}
			continue;
		}
		for (int32 Index = Run.FirstVertex; Index + 1 < Run.FirstVertex + Run.NumVertices; Index += 2)
		{
			const FCanvasVertex& A = Vertices[Index];
			const FCanvasVertex& B = Vertices[Index + 1];
			Emit(A.X, A.Y, A);
			Emit(B.X, A.Y, A);
			Emit(B.X, B.Y, A);
			Emit(A.X, A.Y, A);
			Emit(B.X, B.Y, A);
			Emit(A.X, B.Y, A);
		}
	}
	const TArray<FColor> FromSprites = Rasterize(Sprites, Environment);
	const TArray<FColor> FromTriangles = Rasterize(Triangles, Environment);
	int32 NumDifferent = 0;
	int32 NumDrawn = 0;
	for (int32 Index = 0; Index < FromSprites.Num(); ++Index)
	{
		NumDifferent += FromSprites[Index] == FromTriangles[Index] ? 0 : 1;
		NumDrawn += FromTriangles[Index] == FromTriangles[0] ? 0 : 1;
	}
	int32 NumSpriteVertices = 0;
	for (const FGSRegisterWrite& Write : Sprites.GetWrites())
	{
		NumSpriteVertices += Write.Register == EGSRegister::XYZ2 ? 1 : 0;
	}
	UE_LOG(LogTemp, Display, "%s",
		*FString::Printf("Canvas: %d vertices as sprites, %d as triangles; %d pixel(s) drawn, %d differ",
			NumSpriteVertices, NumTriangleVertices, NumDrawn, NumDifferent));
	TestTrue("Something drawn", NumDrawn > 1000);
	TestEqual("The same frame", NumDifferent, 0);
	TestTrue("About a third of the vertices", NumSpriteVertices * 3 <= NumTriangleVertices + 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneCellsAndPortalsTest, "System.Renderer.GS.Scene.CellsAndPortals",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneCellsAndPortalsTest::RunTest(const FString& Parameters)
{
	// Two rooms, A (x 0 to 10 m) and B (10 to 20 m), a door between them at y 4.5 to 5.5 m, a cube in each; a camera in
	// A looking at the wall beside the door leaves B's cube out whatever the frustum says; looking through the door, it
	// is drawn; a cube outside both rooms is always drawn; without the cells everything is.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	const auto SpawnCell = [&World](const TCHAR* Name, float X)
	{
		AVisibilityCellVolume* Cell =
			World.SpawnActor<AVisibilityCellVolume>(FVector(X + 500.0f, 500.0f, 150.0f), FRotator(0.0f, 0.0f, 0.0f));
		Cell->CellName = FName(Name);
		Cell->SetActorScale3D(FVector(1000.0f, 1000.0f, 300.0f) / (2.0f * AVolume::BrushExtent));
		return Cell;
	};
	(void)SpawnCell(TEXT("A"), 0.0f);
	(void)SpawnCell(TEXT("B"), 1000.0f);
	AVisibilityPortal* Door =
		World.SpawnActor<AVisibilityPortal>(FVector(1000.0f, 500.0f, 100.0f), FRotator(0.0f, 0.0f, 0.0f));
	Door->CellA = FName(TEXT("A"));
	Door->CellB = FName(TEXT("B"));
	const FVector DoorCorners[4] = {FVector(1000.0f, 450.0f, 0.0f), FVector(1000.0f, 550.0f, 0.0f),
		FVector(1000.0f, 550.0f, 200.0f), FVector(1000.0f, 450.0f, 200.0f)};
	Door->Corners.Append(DoorCorners, 4);
	World.Scene->UpdateVisibilityCells();
	UStaticMesh* Cube = NewObject<UStaticMesh>();
	(void)Cube->BuildFromMeshData(MakeCube());
	(void)SpawnMesh(World, Cube, FVector(800.0f, 200.0f, 100.0f), FVector(0.5f), FLinearColor::White);
	(void)SpawnMesh(World, Cube, FVector(1500.0f, 800.0f, 100.0f), FVector(0.5f), FLinearColor::White);
	(void)SpawnMesh(World, Cube, FVector(1500.0f, 1200.0f, 100.0f), FVector(0.5f), FLinearColor::White);

	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	FGSCommandList List;
	// From A, near the wall 4 m beside the door, looking at the wall (B's cube is behind it, in the frustum; the door
	// is 75 degrees aside, out of the view).
	UCameraComponent& AtWall = MakeCamera(FVector(900.0f, 900.0f, 100.0f), FRotator(0.0f, 0.0f, 0.0f));
	FFrameStats Stats = RenderFrame(World, AtWall, Renderer, List);
	TestEqual("One cell seen", Stats.CellsVisible, 1);
	TestEqual("B's cube left out", Stats.ObjectsCulledByCells, 1);

	// Through the door: B too.
	UCameraComponent& ThroughDoor = MakeCamera(FVector(100.0f, 500.0f, 100.0f), FRotator(0.0f, 0.0f, 0.0f));
	Stats = RenderFrame(World, ThroughDoor, Renderer, List);
	TestEqual("Both cells seen", Stats.CellsVisible, 2);
	TestEqual("Nothing left out by the cells", Stats.ObjectsCulledByCells, 0);

	// Without cells: the frustum alone.
	for (AActor* Actor : TArray<AActor*>(World.PersistentLevel->Actors))
	{
		if (Cast<AVisibilityCellVolume>(Actor) != nullptr || Cast<AVisibilityPortal>(Actor) != nullptr)
		{
			(void)World.DestroyActor(Actor);
		}
	}
	Stats = RenderFrame(World, AtWall, Renderer, List);
	TestEqual("No cells", Stats.CellsVisible, 0);
	TestEqual("Nothing left out", Stats.ObjectsCulledByCells, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSSceneViewModelCullingTest, "System.Renderer.GS.Scene.ViewModelCulling",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGSSceneViewModelCullingTest::RunTest(const FString& Parameters)
{
	// The view model pass culls by its own frustum (N15): a first-person mesh behind the eye is left out, one in front
	// is drawn.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	UStaticMesh* Cube = NewObject<UStaticMesh>();
	(void)Cube->BuildFromMeshData(MakeCube());
	AStaticMeshActor* Weapon =
		SpawnMesh(World, Cube, FVector(100.0f, 0.0f, 0.0f), FVector(0.2f), FLinearColor(0.5f, 0.5f, 0.5f));
	Weapon->GetStaticMeshComponent()->SetRenderAsViewModel(true);
	UCameraComponent& Camera = MakeCamera(FVector::ZeroVector, FRotator(0.0f, 0.0f, 0.0f));
	FGSSceneRenderer Renderer;
	Renderer.GetTextureCache().SetArena(ArenaFirstBlock, ArenaBlocks);
	FGSCommandList List;
	FFrameStats Stats = RenderFrame(World, Camera, Renderer, List);
	TestEqual("In front: drawn", Stats.ObjectsVisible, 1);
	Weapon->SetActorLocation(FVector(-100.0f, 0.0f, 0.0f));
	Stats = RenderFrame(World, Camera, Renderer, List);
	TestEqual("Behind: culled", Stats.ObjectsCulled, 1);
	TestEqual("Behind: nothing drawn", Stats.TrianglesSubmitted, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
