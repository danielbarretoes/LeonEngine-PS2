#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneAddBodyAndClearTest, "System.Engine.PhysScene.AddBodyAndClear",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPhysSceneAddBodyAndClearTest::RunTest(const FString& Parameters)
{
	// AddBody returns sequential ids and Clear removes every body.
	FPhysScene Scene;
	TestEqual("Empty scene", Scene.GetBodies().Num(), 0);
	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	TestEqual("First id", Id, 0);
	TestEqual("One body", Scene.GetBodies().Num(), 1);
	Scene.Clear();
	TestEqual("Cleared", Scene.GetBodies().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneStepAppliesGravityAndSnapsToFloorTest,
	"System.Engine.PhysScene.StepAppliesGravityAndSnapsToFloor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPhysSceneStepAppliesGravityAndSnapsToFloorTest::RunTest(const FString& Parameters)
{
	// A dynamic box falls under gravity and comes to rest on the floor plane.
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({0, EBodyType::Dynamic, 1.0f, true});
	FBodyInstance& Body = Scene.GetBodies()[Id];
	Body.Position = FVector(0.0f, 0.0f, 200.0f);
	Body.HalfExtents = FVector(50.0f, 50.0f, 50.0f);

	FPhysSceneStepParams Params;
	Params.DeltaTime = 1.0f / 60.0f;
	Params.Gravity = 2400.0f;
	Params.FloorZ = 0.0f;
	Params.Skin = 2.0f;

	for (int32 I = 0; I < 180; ++I)
	{
		Scene.Step(Params);
	}

	// Bottom of the AABB ~ FloorZ (center Z ~ HalfExtents.Z).
	TestTrue("Fell below start", Body.Position.Z < 100.0f);
	TestTrue("Above the floor", Body.Position.Z > 0.0f);
	TestEqual("Stayed over the same spot", Body.Position.Y, 0.0f, 1.0e-3f);
	TestEqual("At rest", Body.VelocityZ, 0.0f, 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneStaticBodyDoesNotMoveUnderGravityTest,
	"System.Engine.PhysScene.StaticBodyDoesNotMoveUnderGravity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPhysSceneStaticBodyDoesNotMoveUnderGravityTest::RunTest(const FString& Parameters)
{
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	FBodyInstance& Body = Scene.GetBodies()[Id];
	Body.Position = FVector(100.0f, 300.0f, 200.0f);
	Body.HalfExtents = FVector(50.0f, 50.0f, 50.0f);

	FPhysSceneStepParams Params;
	Params.DeltaTime = 0.1f;
	Params.Gravity = 5000.0f;
	Scene.Step(Params);

	TestEqual("X", Body.Position.X, 100.0f, 1.0e-3f);
	TestEqual("Y", Body.Position.Y, 300.0f, 1.0e-3f);
	TestEqual("Z", Body.Position.Z, 200.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneQuerySupportZUsesBodyTopsTest,
	"System.Engine.PhysScene.QuerySupportZUsesBodyTops",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPhysSceneQuerySupportZUsesBodyTopsTest::RunTest(const FString& Parameters)
{
	// A capsule standing just above a box is supported by the box top.
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	FBodyInstance& Body = Scene.GetBodies()[Id];
	Body.Position = FVector(0.0f, 0.0f, 100.0f);
	Body.HalfExtents = FVector(100.0f, 100.0f, 100.0f); // top at z=200 cm

	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(35.0f, 92.5f);

	const float Support = Scene.QuerySupportZ(Capsule, FVector(0.0f, 0.0f, 210.0f), 0.0f, 35.0f, 2.0f, NoComponentID);
	TestEqual("Support height", Support, 200.0f, 0.1f);
	// Off to the side along Y, the box does not support the capsule.
	const float Beside = Scene.QuerySupportZ(Capsule, FVector(0.0f, 300.0f, 210.0f), 0.0f, 35.0f, 2.0f, NoComponentID);
	TestEqual("Floor beside the box", Beside, 0.0f, 0.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneSlopeRampRisesAlongYawTest, "System.Engine.PhysScene.SlopeRampRisesAlongYaw",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPhysSceneSlopeRampRisesAlongYawTest::RunTest(const FString& Parameters)
{
	// A ramp rises along +X; a yaw of 90 degrees turns the rise toward +Y (a UE yaw). The normal leans against it.
	FPhysScene Scene;
	(void)Scene.AddSlopeRamp(FVector::ZeroVector, FVector(400.0f, 400.0f, 400.0f), 30.0f);
	(void)Scene.AddSlopeRamp(FVector::ZeroVector, FVector(400.0f, 400.0f, 400.0f), 30.0f, 90.0f);
	const float S = FMath::Sin(FMath::DegreesToRadians(30.0f));
	const float C = FMath::Cos(FMath::DegreesToRadians(30.0f));
	TestTrue("Rises along +X", Scene.GetSlopePlanes()[0].Normal.Equals(FVector(-S, 0.0f, C), 1.0e-5f));
	TestTrue("Rises along +Y", Scene.GetSlopePlanes()[1].Normal.Equals(FVector(0.0f, -S, C), 1.0e-5f));

	// The support under a capsule on the second ramp grows with Y.
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(35.0f, 92.5f);
	FPhysScene Turned;
	(void)Turned.AddSlopeRamp(FVector::ZeroVector, FVector(400.0f, 400.0f, 400.0f), 30.0f, 90.0f);
	const float Low = Turned.QuerySupportZ(Capsule, FVector(0.0f, -100.0f, 0.0f), -1000.0f, 35.0f, 2.0f, NoComponentID);
	const float High =
		Turned.QuerySupportZ(Capsule, FVector(0.0f, 100.0f, 200.0f), -1000.0f, 35.0f, 2.0f, NoComponentID);
	TestEqual("Low side", Low, -100.0f * (S / C), 0.1f);
	TestEqual("High side", High, 100.0f * (S / C), 0.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneResolveCapsuleSidesPushesOutOfAabbTest,
	"System.Engine.PhysScene.ResolveCapsuleSidesPushesOutOfAabb",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPhysSceneResolveCapsuleSidesPushesOutOfAabbTest::RunTest(const FString& Parameters)
{
	// A capsule overlapping the side of a box is pushed out in XY.
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	FBodyInstance& Body = Scene.GetBodies()[Id];
	Body.Position = FVector(0.0f, 0.0f, 90.0f);
	Body.HalfExtents = FVector(50.0f, 50.0f, 90.0f);

	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(35.0f, 92.5f);

	FVector Feet(10.0f, 0.0f, 0.0f);
	FCapsuleContactParams Contact{};
	Scene.ResolveCapsuleSides(Capsule, Feet, FVector2D(0.0f, 0.0f), Contact, NoComponentID, false);

	const float DistXY = FMath::Sqrt((Feet.X * Feet.X) + (Feet.Y * Feet.Y));
	TestTrue("Pushed out", DistXY > 40.0f);
	TestEqual("Height kept", Feet.Z, 0.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysSceneApplyCapsuleSweepPushMovesDynamicWithoutPenetrationTest,
	"System.Engine.PhysScene.ApplyCapsuleSweepPushMovesDynamicWithoutPenetration",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPhysSceneApplyCapsuleSweepPushMovesDynamicWithoutPenetrationTest::RunTest(const FString& Parameters)
{
	// A sweep hit facing the wish direction shoves the dynamic body; a hit facing away does not.
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({7, EBodyType::Dynamic, 1.0f, true});
	FBodyInstance& Body = Scene.GetBodies()[Id];
	Body.Position = FVector(200.0f, 0.0f, 50.0f);
	Body.HalfExtents = FVector(50.0f, 50.0f, 50.0f);
	Body.Mass = 1.0f;

	TestTrue(
		"Pushed", Scene.ApplyCapsuleSweepPush(7, FVector2D(1.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), 0.85f, 1800.0f));
	TestTrue("Gained velocity", Body.VelXY.X > 10.0f);
	TestTrue("Moved", Body.Position.X > 200.0f);

	TestFalse("Not pushed from behind",
		Scene.ApplyCapsuleSweepPush(7, FVector2D(1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), 0.85f, 1800.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
