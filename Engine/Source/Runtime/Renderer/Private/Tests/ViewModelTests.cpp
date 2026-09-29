#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Effects/WorldEffects.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Primitives.h"
#include "ScenePrivate.h"
#include "SceneView.h"
#include "StaticMeshSceneProxy.h"
#include "Tests/ScopedTestWorld.h"
#include "WorldEffectsGeometry.h"

#if WITH_DEV_AUTOMATION_TESTS

// P18's rendering additions that need no GPU: which static meshes a view draws in the scene's passes and which in the
// view model pass (owner-only, owner-hidden and view model primitives), and the impact mark pool and the effects'
// geometry. The engine frames (LeonGame's captures) show that nothing changes without them.

namespace
{

	/** A static mesh actor showing Mesh, owned by Owner. */
	UStaticMeshComponent* SpawnMesh(UWorld& World, UStaticMesh* Mesh, AActor* Owner)
	{
		AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>();
		Actor->SetOwner(Owner);
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		(void)Component->SetStaticMesh(Mesh);
		return Component;
	}

	/** GatherPrimitives' split of the static meshes for a view whose actor is ViewActor. */
	void Split(const UWorld& World, const AActor* ViewActor, TArray<const FStaticMeshSceneProxy*>& OutWorld,
		TArray<const FStaticMeshSceneProxy*>& OutViewModel)
	{
		FSceneViewInitOptions Options;
		Options.ViewActor = ViewActor;
		const FSceneView View(Options);
		FMemMark Mark(FMemStack::Get());
		FScratchpadMark ScratchpadMark;
		FSceneRenderList<const FStaticMeshSceneProxy*> WorldMeshes;
		FSceneRenderList<const FStaticMeshSceneProxy*> ViewModelMeshes;
		FSceneRenderList<const FPrimitiveSceneInfo*> WorldSkeletalMeshes;
		FSceneRenderList<const FPrimitiveSceneInfo*> ViewModelSkeletalMeshes;
		FVisibilityCellGraph::FVisibleCells Everything;
		(void)World.Scene->GetRenderScene()->GatherPrimitives(
			View, Everything, WorldMeshes, ViewModelMeshes, WorldSkeletalMeshes, ViewModelSkeletalMeshes);
		OutWorld.Reset();
		OutViewModel.Reset();
		for (const FStaticMeshSceneProxy* Proxy : WorldMeshes)
		{
			OutWorld.Add(Proxy);
		}
		for (const FStaticMeshSceneProxy* Proxy : ViewModelMeshes)
		{
			OutViewModel.Add(Proxy);
		}
	}

	/** True when List holds Component's proxy. */
	bool Holds(const TArray<const FStaticMeshSceneProxy*>& List, const UStaticMeshComponent* Component)
	{
		return Component->SceneProxy != nullptr &&
			List.Contains(static_cast<const FStaticMeshSceneProxy*>(Component->SceneProxy));
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRendererViewModelOnlyWhenFlaggedTest, "System.Renderer.ViewModel.OnlyWhenFlagged",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FRendererViewModelOnlyWhenFlaggedTest::RunTest(const FString& Parameters)
{
	// Only a primitive flagged bRenderAsViewModel leaves the scene's passes for the view model pass (and casts no
	// shadow); an owner-only one is drawn in its owner's view only, an owner-hidden one in every other view; a plain
	// one is drawn everywhere, as before.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	if (!TestNotNull("A scene", World.Scene))
	{
		return false;
	}
	UStaticMesh* Cube = NewObject<UStaticMesh>();
	(void)Cube->BuildFromMeshData(MakeCube());
	APawn* Player = World.SpawnActor<APawn>();
	UStaticMeshComponent* Wall = SpawnMesh(World, Cube, nullptr);
	UStaticMeshComponent* Weapon = SpawnMesh(World, Cube, Player);
	UStaticMeshComponent* Body = SpawnMesh(World, Cube, Player);

	TArray<const FStaticMeshSceneProxy*> WorldMeshes;
	TArray<const FStaticMeshSceneProxy*> ViewModelMeshes;
	Split(World, Player, WorldMeshes, ViewModelMeshes);
	TestEqual("Nothing flagged: all in the scene", WorldMeshes.Num(), 3);
	TestEqual("Nothing flagged: no view model", ViewModelMeshes.Num(), 0);

	Weapon->SetRenderAsViewModel(true);
	Weapon->SetOnlyOwnerSee(true);
	Body->SetOwnerNoSee(true);
	TestFalse("A view model casts no shadow", Weapon->SceneProxy->CastsDynamicShadow());
	Split(World, Player, WorldMeshes, ViewModelMeshes);
	TestTrue("The owner's view: the wall", WorldMeshes.Num() == 1 && Holds(WorldMeshes, Wall));
	TestTrue("The owner's view: its weapon in the view model pass",
		ViewModelMeshes.Num() == 1 && Holds(ViewModelMeshes, Weapon));

	Split(World, nullptr, WorldMeshes, ViewModelMeshes);
	TestTrue("Another view: the wall and the body",
		WorldMeshes.Num() == 2 && Holds(WorldMeshes, Wall) && Holds(WorldMeshes, Body));
	TestEqual("Another view: no weapon", ViewModelMeshes.Num(), 0);

	Weapon->SetOnlyOwnerSee(false);
	Split(World, nullptr, WorldMeshes, ViewModelMeshes);
	TestTrue("A view model for every view", ViewModelMeshes.Num() == 1 && Holds(ViewModelMeshes, Weapon));
	TestFalse("Never in the scene's passes", Holds(WorldMeshes, Weapon));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRendererImpactMarkPoolRecyclesTest, "System.Renderer.Effects.ImpactMarkPoolRecycles",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FRendererImpactMarkPoolRecyclesTest::RunTest(const FString& Parameters)
{
	// The pool keeps 64 marks: the 65th and later take the oldest's slot; an expired mark frees its slot; each active
	// mark is two triangles lifted off its surface.
	FImpactMarkPool Pool;
	for (int32 Index = 0; Index < 70; ++Index)
	{
		FImpactMark Mark;
		Mark.Location = FVector(static_cast<float>(Index), 0.0f, 0.0f);
		(void)Pool.AddMark(Mark);
	}
	TestEqual("Full", Pool.Num(), FImpactMarkPool::MaxMarks);
	TestEqual("Never grows", Pool.GetMarks().Num(), FImpactMarkPool::MaxMarks);
	uint32 Oldest = TNumericLimits<uint32>::Max();
	for (const FImpactMark& Mark : Pool.GetMarks())
	{
		Oldest = FMath::Min(Oldest, Mark.Serial);
	}
	TestEqual("The six oldest went", Oldest, 6u);
	TestTrue("Slot 0 recycled", Pool.GetMarks()[0].Location.X == 64.0f);
	TestTrue("Slot 5 recycled", Pool.GetMarks()[5].Location.X == 69.0f);
	TestTrue("Slot 6 kept", Pool.GetMarks()[6].Location.X == 6.0f);

	FImpactMark Short;
	Short.LifeSpan = 1.0f;
	Short.Color.A = 0.5f;
	const int32 ShortSlot = Pool.AddMark(Short);
	TestEqual("The next oldest's slot", ShortSlot, 6);
	Pool.Tick(0.9f);
	TestEqual("Fading", Pool.GetMarks()[ShortSlot].GetOpacity(), 0.25f, 1.0e-4f);
	Pool.Tick(0.2f);
	TestEqual("Expired", Pool.Num(), FImpactMarkPool::MaxMarks - 1);
	FImpactMark Next;
	TestEqual("A free slot first", Pool.AddMark(Next), ShortSlot);

	TArray<FWorldEffectVertex> Vertices;
	FWorldEffectsGeometry::BuildImpactMarkVertices(Pool, Vertices);
	TestEqual("Two triangles a mark", Vertices.Num(), 6 * FImpactMarkPool::MaxMarks);
	TestEqual("Lifted off the surface", Vertices[0].Position.Z, 0.1f, 1.0e-5f);

	FTracerBatch Tracers;
	FTracer Tracer;
	Tracer.Start = FVector(0.0f, 0.0f, 100.0f);
	Tracer.End = FVector(1000.0f, 0.0f, 100.0f);
	Tracers.AddTracer(Tracer);
	FWorldEffectsGeometry::BuildTracerVertices(Tracers, FVector(500.0f, 0.0f, 300.0f), Vertices);
	TestEqual("A ribbon", Vertices.Num(), 6);
	float MaxY = 0.0f;
	for (const FWorldEffectVertex& Vertex : Vertices)
	{
		MaxY = FMath::Max(MaxY, FMath::Abs(Vertex.Position.Y));
	}
	TestEqual("Facing the camera above it: across Y", MaxY, Tracer.Width * 0.5f, 1.0e-4f);
	Tracers.Tick(Tracer.LifeSpan);
	TestTrue("Gone after its life span", Tracers.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRendererEffectSpritesTest, "System.Renderer.Effects.EffectSprites",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FRendererEffectSpritesTest::RunTest(const FString& Parameters)
{
	// The world's effect sprites (a smoke grenade's puffs): they fade in and out, a pool of MaxSprites recycles the
	// oldest and forgets a removed one; each is two triangles square to the view, the farthest drawn first.
	FEffectSpritePool Pool;
	FEffectSprite Sprite;
	Sprite.Size = 200.0f;
	Sprite.Color = FLinearColor(0.6f, 0.6f, 0.6f, 0.8f);
	Sprite.LifeSpan = 10.0f;
	Sprite.FadeInTime = 1.0f;
	Sprite.FadeOutTime = 2.0f;
	(void)Pool.AddSprite(Sprite);
	TestEqual("Fading in", Pool.GetSprites()[0].GetOpacity(), 0.0f);
	Pool.Tick(0.5f);
	TestEqual("Half in", Pool.GetSprites()[0].GetOpacity(), 0.4f, 1.0e-4f);
	Pool.Tick(8.5f);
	TestEqual("Half out", Pool.GetSprites()[0].GetOpacity(), 0.4f, 1.0e-4f);
	Pool.Tick(1.1f);
	TestTrue("Gone after its life span", Pool.IsEmpty());

	TArray<uint32> Serials;
	for (int32 Index = 0; Index < FEffectSpritePool::MaxSprites + 8; ++Index)
	{
		Serials.Add(Pool.AddSprite(Sprite));
	}
	TestEqual("Full", Pool.Num(), FEffectSpritePool::MaxSprites);
	TestEqual("Never grows", Pool.GetSprites().Num(), FEffectSpritePool::MaxSprites);
	Pool.RemoveSprite(Serials[0]);
	TestEqual("The oldest was recycled: removing it changes nothing", Pool.Num(), FEffectSpritePool::MaxSprites);
	Pool.RemoveSprite(Serials.Last());
	TestEqual("A removed one", Pool.Num(), FEffectSpritePool::MaxSprites - 1);

	// A view at the origin looking along +X (UE view space: x right, y up, z forward).
	Pool.Clear();
	Sprite.FadeInTime = 0.0f;
	Sprite.Location = FVector(500.0f, 0.0f, 100.0f);
	(void)Pool.AddSprite(Sprite);
	Sprite.Location = FVector(1000.0f, 0.0f, 100.0f);
	(void)Pool.AddSprite(Sprite);
	const FMatrix ViewMatrix(FPlane(0.0f, 0.0f, 1.0f, 0.0f), FPlane(1.0f, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, 1.0f, 0.0f, 0.0f), FPlane(0.0f, 0.0f, 0.0f, 1.0f));
	TArray<FWorldEffectVertex> Vertices;
	FWorldEffectsGeometry::BuildEffectSpriteVertices(Pool, ViewMatrix, FVector::ZeroVector, Vertices);
	if (!TestEqual("Two triangles a sprite", Vertices.Num(), 12))
	{
		return false;
	}
	bool bSquareToTheView = true;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FVector& Corner = Vertices[Index].Position;
		bSquareToTheView &= FMath::IsNearlyEqual(Corner.X, 1000.0f) &&
			FMath::IsNearlyEqual(FMath::Abs(Corner.Y), 100.0f) &&
			FMath::IsNearlyEqual(FMath::Abs(Corner.Z - 100.0f), 100.0f);
	}
	TestTrue("The farthest first, square to the view", bSquareToTheView);
	TestEqual("Its opacity", Vertices[0].Color.A, 0.8f, 1.0e-4f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
