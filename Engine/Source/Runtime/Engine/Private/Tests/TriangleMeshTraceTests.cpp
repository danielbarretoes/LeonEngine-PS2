#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "StaticMesh.h"
#include "TriangleCollision.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTriangleMeshTraceLineTraceAndQuerySupportYUseTriangleMeshSurfaceTest,
	"System.Engine.TriangleMeshTrace.LineTraceAndQuerySupportYUseTriangleMeshSurface",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTriangleMeshTraceLineTraceAndQuerySupportYUseTriangleMeshSurfaceTest::RunTest(const FString& Parameters)
{
	// A collidable static mesh syncs as a TriangleMesh body; line traces and capsule support use its surface.
	ULevel Level;
	FMeshData Data;
	// Flat plane at y=0.5 covering xz [-2,2]
	const FVector Up(0.0f, 1.0f, 0.0f);
	const FVector4 Tangent(1.0f, 0.0f, 0.0f, 1.0f);
	Data.Vertices.Add(FVertex(FVector(-2.0f, 0.5f, -2.0f), Up, FVector2D(0.0f, 0.0f), Tangent));
	Data.Vertices.Add(FVertex(FVector(2.0f, 0.5f, -2.0f), Up, FVector2D(1.0f, 0.0f), Tangent));
	Data.Vertices.Add(FVertex(FVector(2.0f, 0.5f, 2.0f), Up, FVector2D(1.0f, 1.0f), Tangent));
	Data.Vertices.Add(FVertex(FVector(-2.0f, 0.5f, 2.0f), Up, FVector2D(0.0f, 1.0f), Tangent));
	Data.Indices = {0, 1, 2, 0, 2, 3};
	Data.Submeshes.Add(FMeshSection{0, 6, 0});

	UStaticMeshComponent Component{};
	Component.Mesh = MakeShared<UStaticMesh>(UStaticMesh::CreateCpu(Data));
	Component.bCollisionEnabled = true;
	Level.GetStaticMeshes().Add(MoveTemp(Component));

	FPhysScene Scene;
	Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.SyncFromLevel(Level);
	TestTrue("TriangleMesh body", Scene.GetBodies()[0].CollisionShape == EBodyCollisionShape::TriangleMesh);

	FHitResult Hit{};
	const bool bHit = Scene.LineTraceSingleByChannel(
		Hit, FVector(0.0f, 3.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f), ECollisionChannel::WorldStatic);
	TestTrue("Trace hit", bHit);
	TestTrue("Blocking hit", Hit.bBlockingHit);
	TestEqual("Impact Y", Hit.ImpactPoint.Y, 0.5f, 2.0e-2f);
	TestTrue("Normal points up", Hit.ImpactNormal.Y > 0.5f);

	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(0.35f, 0.5f);
	const float Support = Scene.QuerySupportY(Capsule, FVector(0.0f, 1.0f, 0.0f), 0.0f, 0.4f, 0.02f, ULevel::Npos);
	TestEqual("Support height", Support, 0.5f, 5.0e-2f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
