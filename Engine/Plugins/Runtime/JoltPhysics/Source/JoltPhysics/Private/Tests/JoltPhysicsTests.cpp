#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "Tests/ScopedTestWorld.h"

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
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	TestTrue("Starts on Arcade", World.GetPhysicsScene().GetBackend() == EPhysicsBackend::Arcade);

	World.SetPhysicsBackend(EPhysicsBackend::Jolt);
	TestTrue("Switched to Jolt", World.GetPhysicsScene().GetBackend() == EPhysicsBackend::Jolt);

	const int32 Id = World.GetPhysicsScene().AddBody({0, EBodyType::Dynamic, 8.0f, true});
	FBodyInstance& Body = World.GetPhysicsScene().GetBodies()[Id];
	Body.Position = FVector(0.0f, 0.0f, 250.0f);
	Body.HalfExtents = FVector(40.0f, 40.0f, 40.0f);

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 2400.0f;
	Params.FloorZ = 0.0f;
	Params.WalkBounds = 5000.0f;
	for (int32 I = 0; I < 180; ++I)
	{
		World.GetPhysicsScene().Step(Params);
	}
	TestTrue("Fell", Body.Position.Z < 110.0f);
	TestTrue("Above the floor", Body.Position.Z > 20.0f);
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
	Body.Position = FVector(0.0f, 0.0f, 300.0f);
	Body.HalfExtents = FVector(50.0f, 50.0f, 50.0f);

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 2400.0f;
	Params.FloorZ = 0.0f;
	Params.WalkBounds = 10000.0f;

	for (int32 I = 0; I < 240; ++I)
	{
		Scene.Step(Params);
	}

	TestTrue("Fell", Body.Position.Z < 120.0f);
	TestTrue("Above the floor", Body.Position.Z > 30.0f);
	TestTrue("At rest", FMath::Abs(Body.VelocityZ) < 100.0f);
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
	Ground.Position = FVector(0.0f, 0.0f, 50.0f);
	Ground.HalfExtents = FVector(200.0f, 200.0f, 50.0f);

	const int32 BoxId = Scene.AddBody({1, EBodyType::Dynamic, 5.0f, true});
	FBodyInstance& Box = Scene.GetBodies()[BoxId];
	Box.Position = FVector(0.0f, 0.0f, 400.0f);
	Box.HalfExtents = FVector(40.0f, 40.0f, 40.0f);

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 2400.0f;
	Params.FloorZ = -1000.0f; // below the ground so the static box is the support
	Params.WalkBounds = 10000.0f;

	for (int32 I = 0; I < 300; ++I)
	{
		Scene.Step(Params);
	}

	// The dynamic box centre settles near the ground top (100 cm) + half extents (40 cm) = 140 cm.
	TestEqual("Rest height", Box.Position.Z, 140.0f, 35.0f);
	TestTrue("At rest", FMath::Abs(Box.VelocityZ) < 150.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoltRestsOnTriangleMeshTest, "System.JoltPhysics.Step.RestsOnTriangleMesh",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJoltRestsOnTriangleMeshTest::RunTest(const FString& Parameters)
{
	// A static level mesh becomes a Jolt triangle mesh and a dropped box rests on it.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	FMeshData Data;
	// Flat plane at z=100 cm covering xy [-300, 300] cm
	const FVector Up(0.0f, 0.0f, 1.0f);
	const FVector4 Tangent(1.0f, 0.0f, 0.0f, 1.0f);
	Data.Vertices.Add(FVertex(FVector(-300.0f, -300.0f, 100.0f), Up, FVector2D(0, 0), Tangent));
	Data.Vertices.Add(FVertex(FVector(300.0f, -300.0f, 100.0f), Up, FVector2D(1, 0), Tangent));
	Data.Vertices.Add(FVertex(FVector(300.0f, 300.0f, 100.0f), Up, FVector2D(1, 1), Tangent));
	Data.Vertices.Add(FVertex(FVector(-300.0f, 300.0f, 100.0f), Up, FVector2D(0, 1), Tangent));
	Data.Indices = {0, 1, 2, 0, 2, 3};
	Data.Submeshes.Add(FMeshSection{0, 6, 0});

	UStaticMeshComponent& Component = *World.SpawnActor<AStaticMeshActor>()->GetStaticMeshComponent();
	UStaticMesh* Mesh = NewObject<UStaticMesh>();
	(void)Mesh->BuildFromMeshData(Data);
	(void)Component.SetStaticMesh(Mesh);
	Component.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	World.SetPhysicsBackend(EPhysicsBackend::Jolt);
	FPhysScene& Scene = World.GetPhysicsScene();
	const int32 BoxId = Scene.AddBody({1, EBodyType::Dynamic, 5.0f, true});
	FBodyInstance& Box = Scene.GetBodies()[BoxId];
	Box.Position = FVector(0.0f, 0.0f, 500.0f);
	Box.HalfExtents = FVector(35.0f, 35.0f, 35.0f);

	Scene.RebuildRigidWorld();
	TestTrue("Triangle mesh", Scene.GetBodies()[0].CollisionShape == EBodyCollisionShape::TriangleMesh);

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 2400.0f;
	Params.FloorZ = -2000.0f;
	Params.WalkBounds = 10000.0f;

	for (int32 I = 0; I < 360; ++I)
	{
		Scene.Step(Params);
	}

	// The plane at z=100 cm + half extents 35 cm = 135 cm.
	TestEqual("Rest height", Box.Position.Z, 135.0f, 45.0f);
	TestTrue("At rest", FMath::Abs(Box.VelocityZ) < 200.0f);
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
	Body.Position = FVector(0.0f, 0.0f, 50.0f);
	Body.HalfExtents = FVector(50.0f, 50.0f, 50.0f);

	FCollisionQueryParams Params;
	Params.bTraceFloorPlane = false;
	FHitResult Hit{};
	const bool bHit = Scene.LineTraceSingleByChannel(
		Hit, FVector(0.0f, -200.0f, 50.0f), FVector(0.0f, 200.0f, 50.0f), ECollisionChannel::WorldStatic, Params);
	if (!TestTrue("Trace hit", bHit))
	{
		return false;
	}
	TestTrue("Blocking", Hit.bBlockingHit);
	TestEqual("Impact Y", Hit.ImpactPoint.Y, -50.0f, 8.0f);
	TestTrue("Normal faces the trace", Hit.ImpactNormal.Y < -0.5f);
	TestEqual("Normal has no height", Hit.ImpactNormal.Z, 0.0f, 1.0e-3f);
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
	Body.Position = FVector(0.0f, 0.0f, 50.0f);
	Body.HalfExtents = FVector(50.0f, 50.0f, 50.0f);

	FCollisionQueryParams Params;
	Params.bTraceFloorPlane = false;
	FHitResult Hit{};
	const bool bHit = Scene.SphereTraceSingleByChannel(Hit, FVector(0.0f, -300.0f, 50.0f), FVector(0.0f, 300.0f, 50.0f),
		25.0f, ECollisionChannel::WorldStatic, Params);
	if (!TestTrue("Trace hit", bHit))
	{
		return false;
	}
	TestTrue("Blocking", Hit.bBlockingHit);
	TestEqual("Sweep centre Y", Hit.Location.Y, -75.0f, 12.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJoltTracesKeepWorldAxesTest, "System.JoltPhysics.Trace.KeepsWorldAxes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FJoltTracesKeepWorldAxesTest::RunTest(const FString& Parameters)
{
	// Jolt runs Y up in metres behind the boundary: a downward trace onto a box top reports a +Z normal in cm, and a
	// vertical capsule (half height along Z) fits under a low ceiling only by its height.
	FPhysScene Scene(EPhysicsBackend::Jolt);
	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	FBodyInstance& Body = Scene.GetBodies()[Id];
	Body.Position = FVector(0.0f, 0.0f, 50.0f);
	Body.HalfExtents = FVector(100.0f, 300.0f, 50.0f);

	FCollisionQueryParams Params;
	Params.bTraceFloorPlane = false;
	FHitResult Hit{};
	if (!TestTrue("Line hit",
			Scene.LineTraceSingleByChannel(Hit, FVector(0.0f, 200.0f, 300.0f), FVector(0.0f, 200.0f, -100.0f),
				ECollisionChannel::WorldStatic, Params)))
	{
		return false;
	}
	TestEqual("Impact on the top", Hit.ImpactPoint.Z, 100.0f, 2.0f);
	TestTrue("Up normal", Hit.ImpactNormal.Equals(FVector(0.0f, 0.0f, 1.0f), 1.0e-3f));

	// A capsule (radius 20, half height 60) swept down onto the top stops with its centre radius + half height above
	// it.
	FHitResult CapsuleHit{};
	if (!TestTrue("Capsule hit",
			Scene.CapsuleTraceSingleByChannel(CapsuleHit, FVector(0.0f, 0.0f, 400.0f), FVector(0.0f, 0.0f, 0.0f), 20.0f,
				60.0f, ECollisionChannel::WorldStatic, Params)))
	{
		return false;
	}
	TestEqual("Capsule centre height", CapsuleHit.Location.Z, 100.0f + 20.0f + 60.0f, 3.0f);
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
