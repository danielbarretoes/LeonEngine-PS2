#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionQueryLineTraceSingleByChannelHitsStaticAabbTest,
	"System.Engine.CollisionQuery.LineTraceSingleByChannelHitsStaticAabb",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionQueryLineTraceSingleByChannelHitsStaticAabbTest::RunTest(const FString& Parameters)
{
	// A horizontal line along +Y hits the near face of a static box, whose normal faces the trace start.
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[Id].Position = FVector(0.0f, 0.0f, 50.0f);
	Scene.GetBodies()[Id].HalfExtents = FVector(50.0f, 50.0f, 50.0f);

	FHitResult Hit{};
	const bool bHit = Scene.LineTraceSingleByChannel(
		Hit, FVector(0.0f, -200.0f, 50.0f), FVector(0.0f, 200.0f, 50.0f), ECC_WorldStatic);
	TestTrue("Trace hit", bHit);
	TestTrue("Blocking hit", Hit.bBlockingHit);
	TestTrue("Hit before the end", Hit.Time < 1.0f);
	TestEqual("Normal Y", Hit.ImpactNormal.Y, -1.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionQueryLineTraceSingleByChannelFiltersByChannelTest,
	"System.Engine.CollisionQuery.LineTraceSingleByChannelFiltersByChannel",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionQueryLineTraceSingleByChannelFiltersByChannelTest::RunTest(const FString& Parameters)
{
	// A dynamic body is invisible to WorldStatic traces and hit by WorldDynamic traces.
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({0, EBodyType::Dynamic, 1.0f, true});
	Scene.GetBodies()[Id].Position = FVector(0.0f, 0.0f, 50.0f);
	Scene.GetBodies()[Id].HalfExtents = FVector(50.0f, 50.0f, 50.0f);

	FHitResult Hit{};
	const bool bStaticHit = Scene.LineTraceSingleByChannel(
		Hit, FVector(0.0f, -200.0f, 50.0f), FVector(0.0f, 200.0f, 50.0f), ECC_WorldStatic);
	TestFalse("WorldStatic misses", bStaticHit);
	const bool bDynamicHit = Scene.LineTraceSingleByChannel(
		Hit, FVector(0.0f, -200.0f, 50.0f), FVector(0.0f, 200.0f, 50.0f), ECC_WorldDynamic);
	TestTrue("WorldDynamic hits", bDynamicHit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionQuerySphereTraceSingleByChannelHitsFloorPlaneTest,
	"System.Engine.CollisionQuery.SphereTraceSingleByChannelHitsFloorPlane",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionQuerySphereTraceSingleByChannelHitsFloorPlaneTest::RunTest(const FString& Parameters)
{
	// A downward sphere trace hits the virtual floor plane at FloorZ.
	FPhysScene Scene;
	FCollisionQueryParams Params{};
	Params.bTraceFloorPlane = true;
	Params.FloorZ = 0.0f;

	FHitResult Hit{};
	const bool bHit = Scene.SphereTraceSingleByChannel(
		Hit, FVector(0.0f, 0.0f, 100.0f), FVector(0.0f, 0.0f, -100.0f), 35.0f, ECC_Visibility, Params);
	TestTrue("Trace hit", bHit);
	TestTrue("Floor plane hit", Hit.bFloorPlane);
	TestEqual("Impact Z", Hit.ImpactPoint.Z, 0.0f, 0.1f);
	TestTrue("Normal points up", Hit.ImpactNormal.Z > 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionQueryCapsuleTraceSingleByChannelFindsPlatformTopTest,
	"System.Engine.CollisionQuery.CapsuleTraceSingleByChannelFindsPlatformTop",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionQueryCapsuleTraceSingleByChannelFindsPlatformTopTest::RunTest(const FString& Parameters)
{
	// A downward capsule trace stops on the top of a platform.
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[Id].Position = FVector(0.0f, 0.0f, 100.0f);
	Scene.GetBodies()[Id].HalfExtents = FVector(100.0f, 100.0f, 100.0f); // top at z=200 cm

	FHitResult Hit{};
	const float Radius = 35.0f;
	const float HalfHeight = 50.0f;
	const bool bHit = Scene.CapsuleTraceSingleByChannel(
		Hit, FVector(0.0f, 0.0f, 300.0f), FVector(0.0f, 0.0f, 150.0f), Radius, HalfHeight, ECC_Visibility);
	TestTrue("Trace hit", bHit);
	TestTrue("Blocking hit", Hit.bBlockingHit);
	TestTrue("Impact at or below the top", Hit.ImpactPoint.Z <= 200.0f + 1.0f);
	TestTrue("Normal points up", Hit.ImpactNormal.Z > 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionQueryLineTraceMultiByChannelReturnsAllHitsSortedTest,
	"System.Engine.CollisionQuery.LineTraceMultiByChannelReturnsAllHitsSorted",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionQueryLineTraceMultiByChannelReturnsAllHitsSortedTest::RunTest(const FString& Parameters)
{
	// A multi line trace returns both boxes nearest first, and the single trace returns the nearest.
	FPhysScene Scene;
	const int32 NearId = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[NearId].Position = FVector(0.0f, 0.0f, 50.0f);
	Scene.GetBodies()[NearId].HalfExtents = FVector(50.0f, 50.0f, 50.0f);

	const int32 FarId = Scene.AddBody({1, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[FarId].Position = FVector(0.0f, 300.0f, 50.0f);
	Scene.GetBodies()[FarId].HalfExtents = FVector(50.0f, 50.0f, 50.0f);

	FCollisionQueryParams Params{};
	Params.bTraceFloorPlane = false;

	TArray<FHitResult> Hits;
	const bool bMultiHit = Scene.LineTraceMultiByChannel(
		Hits, FVector(0.0f, -200.0f, 50.0f), FVector(0.0f, 500.0f, 50.0f), ECC_WorldStatic, Params);
	TestTrue("Multi trace hit", bMultiHit);
	if (!TestEqual("Hit count", Hits.Num(), 2))
	{
		return false;
	}
	TestTrue("Sorted by time", Hits[0].Time < Hits[1].Time);
	TestEqual("Near mesh index", Hits[0].ComponentID, static_cast<SIZE_T>(0));
	TestEqual("Far mesh index", Hits[1].ComponentID, static_cast<SIZE_T>(1));

	FHitResult Single{};
	const bool bSingleHit = Scene.LineTraceSingleByChannel(
		Single, FVector(0.0f, -200.0f, 50.0f), FVector(0.0f, 500.0f, 50.0f), ECC_WorldStatic, Params);
	TestTrue("Single trace hit", bSingleHit);
	TestEqual("Single matches nearest", Single.Time, Hits[0].Time, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionQuerySphereTraceMultiByChannelIncludesFloorAndBodiesTest,
	"System.Engine.CollisionQuery.SphereTraceMultiByChannelIncludesFloorAndBodies",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionQuerySphereTraceMultiByChannelIncludesFloorAndBodiesTest::RunTest(const FString& Parameters)
{
	// A downward multi sphere trace reports both the box in the way and the floor plane.
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[Id].Position = FVector(0.0f, 0.0f, 200.0f);
	Scene.GetBodies()[Id].HalfExtents = FVector(100.0f, 100.0f, 50.0f); // top at 250 cm, bottom at 150 cm

	FCollisionQueryParams Params{};
	Params.bTraceFloorPlane = true;
	Params.FloorZ = 0.0f;

	TArray<FHitResult> Hits;
	const bool bMultiHit = Scene.SphereTraceMultiByChannel(
		Hits, FVector(0.0f, 0.0f, 400.0f), FVector(0.0f, 0.0f, -100.0f), 25.0f, ECC_Visibility, Params);
	TestTrue("Multi trace hit", bMultiHit);
	if (!TestTrue("At least two hits", Hits.Num() >= 2))
	{
		return false;
	}
	TestTrue("Sorted by time", Hits[0].Time <= Hits.Last().Time);
	bool bAnyFloor = false;
	bool bAnyBody = false;
	for (const FHitResult& H : Hits)
	{
		bAnyFloor = bAnyFloor || H.bFloorPlane;
		bAnyBody = bAnyBody || !H.bFloorPlane;
	}
	TestTrue("Floor hit", bAnyFloor);
	TestTrue("Body hit", bAnyBody);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionQueryCapsuleTraceMultiByChannelReturnsMultipleBlockingHitsTest,
	"System.Engine.CollisionQuery.CapsuleTraceMultiByChannelReturnsMultipleBlockingHits",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionQueryCapsuleTraceMultiByChannelReturnsMultipleBlockingHitsTest::RunTest(const FString& Parameters)
{
	// A downward multi capsule trace through a dynamic and a static box returns both, nearest first.
	FPhysScene Scene;
	const int32 A = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[A].Position = FVector(0.0f, 0.0f, 100.0f);
	Scene.GetBodies()[A].HalfExtents = FVector(50.0f, 50.0f, 50.0f);
	const int32 B = Scene.AddBody({1, EBodyType::Dynamic, 1.0f, true});
	Scene.GetBodies()[B].Position = FVector(0.0f, 0.0f, 300.0f);
	Scene.GetBodies()[B].HalfExtents = FVector(50.0f, 50.0f, 50.0f);

	TArray<FHitResult> Hits;
	const bool bMultiHit = Scene.CapsuleTraceMultiByChannel(
		Hits, FVector(0.0f, 0.0f, 500.0f), FVector::ZeroVector, 20.0f, 30.0f, ECC_Visibility);
	TestTrue("Multi trace hit", bMultiHit);
	if (!TestEqual("Hit count", Hits.Num(), 2))
	{
		return false;
	}
	TestTrue("Sorted by time", Hits[0].Time < Hits[1].Time);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
