#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "StaticMesh.h"
#include "Tests/ScopedTestWorld.h"
#include "TriangleCollision.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTriangleMeshTraceLineTraceAndQuerySupportZUseTriangleMeshSurfaceTest,
	"System.Engine.TriangleMeshTrace.LineTraceAndQuerySupportZUseTriangleMeshSurface",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTriangleMeshTraceLineTraceAndQuerySupportZUseTriangleMeshSurfaceTest::RunTest(const FString& Parameters)
{
	// A collidable static mesh component gets a TriangleMesh body; line traces and capsule support use its surface.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	FMeshData Data;
	// Flat plane at z=50 cm covering xy [-200, 200] cm
	const FVector Up(0.0f, 0.0f, 1.0f);
	const FVector4 Tangent(1.0f, 0.0f, 0.0f, 1.0f);
	Data.Vertices.Add(FVertex(FVector(-200.0f, -200.0f, 50.0f), Up, FVector2D(0.0f, 0.0f), Tangent));
	Data.Vertices.Add(FVertex(FVector(200.0f, -200.0f, 50.0f), Up, FVector2D(1.0f, 0.0f), Tangent));
	Data.Vertices.Add(FVertex(FVector(200.0f, 200.0f, 50.0f), Up, FVector2D(1.0f, 1.0f), Tangent));
	Data.Vertices.Add(FVertex(FVector(-200.0f, 200.0f, 50.0f), Up, FVector2D(0.0f, 1.0f), Tangent));
	Data.Indices = {0, 1, 2, 0, 2, 3};
	Data.Submeshes.Add(FMeshSection{0, 6, 0});

	UStaticMeshComponent& Component = *World.SpawnActor<AStaticMeshActor>()->GetStaticMeshComponent();
	(void)Component.SetStaticMesh(MakeShared<UStaticMesh>(UStaticMesh::CreateCpu(Data)));
	Component.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	const FPhysScene& Scene = World.GetPhysicsScene();
	TestTrue("TriangleMesh body", Scene.GetBodies()[0].CollisionShape == EBodyCollisionShape::TriangleMesh);

	FHitResult Hit{};
	const bool bHit = Scene.LineTraceSingleByChannel(
		Hit, FVector(0.0f, 0.0f, 300.0f), FVector(0.0f, 0.0f, -100.0f), ECollisionChannel::WorldStatic);
	TestTrue("Trace hit", bHit);
	TestTrue("Blocking hit", Hit.bBlockingHit);
	TestEqual("Impact Z", Hit.ImpactPoint.Z, 50.0f, 2.0f);
	TestTrue("Normal points up", Hit.ImpactNormal.Z > 0.5f);

	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(35.0f, 50.0f);
	const float Support = Scene.QuerySupportZ(Capsule, FVector(0.0f, 0.0f, 100.0f), 0.0f, 40.0f, 2.0f, NoComponentID);
	TestEqual("Support height", Support, 50.0f, 5.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
