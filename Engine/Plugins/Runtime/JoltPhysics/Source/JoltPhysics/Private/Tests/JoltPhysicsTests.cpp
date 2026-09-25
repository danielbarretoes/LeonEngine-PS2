#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "StaticMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

	#if defined(LEON_WITH_JOLT) && LEON_WITH_JOLT

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoltBackendReportsJoltTest, "System.JoltPhysics.Backend.ReportsJolt",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJoltBackendReportsJoltTest::RunTest(const FString& Parameters)
{
	// A scene created for Jolt gets the Jolt backend with a rigid-body world.
	FPhysScene Scene(EPhysicsBackend::Jolt);
	TestTrue("Backend", Scene.GetBackend() == EPhysicsBackend::Jolt);
	if (!TestNotNull("Backend interface", Scene.GetBackendIface()))
	{
		return false;
	}
	TestEqual("Name", Scene.GetBackendIface()->GetName(), "Jolt");
	TestTrue("Rigid world", Scene.GetBackendIface()->HasRigidWorld());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoltWorldSwitchesBackendTest, "System.JoltPhysics.World.SwitchesBackend",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJoltWorldSwitchesBackendTest::RunTest(const FString& Parameters)
{
	// SetPhysicsBackend moves the world to Jolt; a dropped box then settles near the floor.
	UWorld World;
	TestTrue("Starts on Arcade", World.GetPhysicsScene().GetBackend() == EPhysicsBackend::Arcade);

	World.SetPhysicsBackend(EPhysicsBackend::Jolt);
	TestTrue("Switched to Jolt", World.GetPhysicsScene().GetBackend() == EPhysicsBackend::Jolt);

	const int32 Id = World.GetPhysicsScene().AddBody({0, EBodyType::Dynamic, 8.0f, true});
	FBodyInstance& Body = World.GetPhysicsScene().GetBodies()[Id];
	Body.Position = FVector(0.0f, 2.5f, 0.0f);
	Body.HalfExtents = FVector(0.4f, 0.4f, 0.4f);

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 24.0f;
	Params.FloorY = 0.0f;
	Params.WalkBounds = 50.0f;
	for (int32 I = 0; I < 180; ++I)
	{
		World.GetPhysicsScene().Step(Params);
	}
	TestTrue("Fell", Body.Position.Y < 1.1f);
	TestTrue("Above the floor", Body.Position.Y > 0.2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoltGravityRestsOnFloorTest, "System.JoltPhysics.Step.GravityRestsOnFloor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJoltGravityRestsOnFloorTest::RunTest(const FString& Parameters)
{
	// Step applies gravity and a dynamic box comes to rest on the floor plane.
	FPhysScene Scene(EPhysicsBackend::Jolt);
	const int32 Id = Scene.AddBody({0, EBodyType::Dynamic, 10.0f, true});
	FBodyInstance& Body = Scene.GetBodies()[Id];
	Body.Position = FVector(0.0f, 3.0f, 0.0f);
	Body.HalfExtents = FVector(0.5f, 0.5f, 0.5f);

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 24.0f;
	Params.FloorY = 0.0f;
	Params.WalkBounds = 100.0f;

	for (int32 I = 0; I < 240; ++I)
	{
		Scene.Step(Params);
	}

	TestTrue("Fell", Body.Position.Y < 1.2f);
	TestTrue("Above the floor", Body.Position.Y > 0.3f);
	TestTrue("At rest", FMath::Abs(Body.VelocityY) < 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoltRestsOnStaticBoxTest, "System.JoltPhysics.Step.RestsOnStaticBox",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJoltRestsOnStaticBoxTest::RunTest(const FString& Parameters)
{
	// A dynamic box dropped onto a static box settles on its top.
	FPhysScene Scene(EPhysicsBackend::Jolt);

	const int32 GroundId = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	FBodyInstance& Ground = Scene.GetBodies()[GroundId];
	Ground.Position = FVector(0.0f, 0.5f, 0.0f);
	Ground.HalfExtents = FVector(2.0f, 0.5f, 2.0f);

	const int32 BoxId = Scene.AddBody({1, EBodyType::Dynamic, 5.0f, true});
	FBodyInstance& Box = Scene.GetBodies()[BoxId];
	Box.Position = FVector(0.0f, 4.0f, 0.0f);
	Box.HalfExtents = FVector(0.4f, 0.4f, 0.4f);

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 24.0f;
	Params.FloorY = -10.0f; // below the ground so the static box is the support
	Params.WalkBounds = 100.0f;

	for (int32 I = 0; I < 300; ++I)
	{
		Scene.Step(Params);
	}

	// The dynamic box centre settles near the ground top (1.0) + half extents (0.4) = 1.4.
	TestEqual("Rest height", Box.Position.Y, 1.4f, 0.35f);
	TestTrue("At rest", FMath::Abs(Box.VelocityY) < 1.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoltRestsOnTriangleMeshTest, "System.JoltPhysics.Step.RestsOnTriangleMesh",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJoltRestsOnTriangleMeshTest::RunTest(const FString& Parameters)
{
	// A static level mesh becomes a Jolt triangle mesh and a dropped box rests on it.
	ULevel Level;
	FMeshData Data;
	// Flat plane at y=1 covering xz [-3,3]
	Data.Vertices.Add(FVertex(FVector(-3.0f, 1.0f, -3.0f), FVector(0, 1, 0), FVector2D(0, 0), FVector4(1, 0, 0, 1)));
	Data.Vertices.Add(FVertex(FVector(3.0f, 1.0f, -3.0f), FVector(0, 1, 0), FVector2D(1, 0), FVector4(1, 0, 0, 1)));
	Data.Vertices.Add(FVertex(FVector(3.0f, 1.0f, 3.0f), FVector(0, 1, 0), FVector2D(1, 1), FVector4(1, 0, 0, 1)));
	Data.Vertices.Add(FVertex(FVector(-3.0f, 1.0f, 3.0f), FVector(0, 1, 0), FVector2D(0, 1), FVector4(1, 0, 0, 1)));
	Data.Indices = {0, 1, 2, 0, 2, 3};
	Data.Submeshes.Add(FMeshSection{0, 6, 0});

	UStaticMeshComponent Component{};
	Component.Mesh = MakeShared<UStaticMesh>(UStaticMesh::CreateCpu(Data));
	Component.bCollisionEnabled = true;
	Level.GetStaticMeshes().Add(MoveTemp(Component));

	FPhysScene Scene(EPhysicsBackend::Jolt);
	Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	const int32 BoxId = Scene.AddBody({1, EBodyType::Dynamic, 5.0f, true});
	FBodyInstance& Box = Scene.GetBodies()[BoxId];
	Box.Position = FVector(0.0f, 5.0f, 0.0f);
	Box.HalfExtents = FVector(0.35f, 0.35f, 0.35f);

	Scene.SyncFromLevel(Level);
	TestTrue("Triangle mesh", Scene.GetBodies()[0].CollisionShape == EBodyCollisionShape::TriangleMesh);

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 24.0f;
	Params.FloorY = -20.0f;
	Params.WalkBounds = 100.0f;

	for (int32 I = 0; I < 360; ++I)
	{
		Scene.Step(Params);
	}

	// The plane at y=1 + half extents 0.35 = 1.35.
	TestEqual("Rest height", Box.Position.Y, 1.35f, 0.45f);
	TestTrue("At rest", FMath::Abs(Box.VelocityY) < 2.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoltLineTraceHitsStaticBoxTest, "System.JoltPhysics.Trace.LineTraceHitsStaticBox",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJoltLineTraceHitsStaticBoxTest::RunTest(const FString& Parameters)
{
	// A line trace through a static box hits its near face with an outward normal.
	FPhysScene Scene(EPhysicsBackend::Jolt);
	TestTrue("Narrow-phase traces", Scene.GetBackendIface()->HasNarrowPhaseTraces());

	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	FBodyInstance& Body = Scene.GetBodies()[Id];
	Body.Position = FVector(0.0f, 0.5f, 0.0f);
	Body.HalfExtents = FVector(0.5f, 0.5f, 0.5f);

	FCollisionQueryParams Params;
	Params.bTraceFloorPlane = false;
	FHitResult Hit{};
	const bool bHit = Scene.LineTraceSingleByChannel(
		Hit, FVector(0.0f, 0.5f, -2.0f), FVector(0.0f, 0.5f, 2.0f), ECollisionChannel::WorldStatic, Params);
	if (!TestTrue("Trace hit", bHit))
	{
		return false;
	}
	TestTrue("Blocking", Hit.bBlockingHit);
	TestEqual("Impact Z", Hit.ImpactPoint.Z, -0.5f, 0.08f);
	TestTrue("Normal faces the trace", Hit.ImpactNormal.Z < -0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoltSphereTraceHitsStaticBoxTest, "System.JoltPhysics.Trace.SphereTraceHitsStaticBox",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJoltSphereTraceHitsStaticBoxTest::RunTest(const FString& Parameters)
{
	// A sphere sweep stops about one radius before the box face.
	FPhysScene Scene(EPhysicsBackend::Jolt);

	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	FBodyInstance& Body = Scene.GetBodies()[Id];
	Body.Position = FVector(0.0f, 0.5f, 0.0f);
	Body.HalfExtents = FVector(0.5f, 0.5f, 0.5f);

	FCollisionQueryParams Params;
	Params.bTraceFloorPlane = false;
	FHitResult Hit{};
	const bool bHit = Scene.SphereTraceSingleByChannel(
		Hit, FVector(0.0f, 0.5f, -3.0f), FVector(0.0f, 0.5f, 3.0f), 0.25f, ECollisionChannel::WorldStatic, Params);
	if (!TestTrue("Trace hit", bHit))
	{
		return false;
	}
	TestTrue("Blocking", Hit.bBlockingHit);
	TestEqual("Sweep centre Z", Hit.Location.Z, -0.75f, 0.12f);
	return true;
}

	#else

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoltDisabledFallsBackToArcadeTest, "System.JoltPhysics.Backend.DisabledFallsBack",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJoltDisabledFallsBackToArcadeTest::RunTest(const FString& Parameters)
{
	// Without the Jolt plugin a Jolt request falls back to the Arcade backend.
	FPhysScene Scene(EPhysicsBackend::Jolt);
	TestTrue("Arcade", Scene.GetBackend() == EPhysicsBackend::Arcade);
	return true;
}

	#endif

#endif // WITH_DEV_AUTOMATION_TESTS
