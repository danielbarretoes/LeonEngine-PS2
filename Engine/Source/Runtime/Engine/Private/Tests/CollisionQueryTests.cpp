#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionQueryLineTraceSingleByChannelHitsStaticAabbTest,
	"System.Engine.CollisionQuery.LineTraceSingleByChannelHitsStaticAabb",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionQueryLineTraceSingleByChannelHitsStaticAabbTest::RunTest(const FString& Parameters)
{
	// A line along +Z hits the near face of a static box, whose normal faces the trace start.
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[Id].Position = FVector(0.0f, 0.5f, 0.0f);
	Scene.GetBodies()[Id].HalfExtents = FVector(0.5f, 0.5f, 0.5f);

	FHitResult Hit{};
	const bool bHit = Scene.LineTraceSingleByChannel(
		Hit, FVector(0.0f, 0.5f, -2.0f), FVector(0.0f, 0.5f, 2.0f), ECollisionChannel::WorldStatic);
	TestTrue("Trace hit", bHit);
	TestTrue("Blocking hit", Hit.bBlockingHit);
	TestTrue("Hit before the end", Hit.Time < 1.0f);
	TestEqual("Normal Z", Hit.ImpactNormal.Z, -1.0f, 1.0e-3f);
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
	Scene.GetBodies()[Id].Position = FVector(0.0f, 0.5f, 0.0f);
	Scene.GetBodies()[Id].HalfExtents = FVector(0.5f, 0.5f, 0.5f);

	FHitResult Hit{};
	const bool bStaticHit = Scene.LineTraceSingleByChannel(
		Hit, FVector(0.0f, 0.5f, -2.0f), FVector(0.0f, 0.5f, 2.0f), ECollisionChannel::WorldStatic);
	TestFalse("WorldStatic misses", bStaticHit);
	const bool bDynamicHit = Scene.LineTraceSingleByChannel(
		Hit, FVector(0.0f, 0.5f, -2.0f), FVector(0.0f, 0.5f, 2.0f), ECollisionChannel::WorldDynamic);
	TestTrue("WorldDynamic hits", bDynamicHit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollisionQuerySphereTraceSingleByChannelHitsFloorPlaneTest,
	"System.Engine.CollisionQuery.SphereTraceSingleByChannelHitsFloorPlane",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCollisionQuerySphereTraceSingleByChannelHitsFloorPlaneTest::RunTest(const FString& Parameters)
{
	// A downward sphere trace hits the virtual floor plane at FloorY.
	FPhysScene Scene;
	FCollisionQueryParams Params{};
	Params.bTraceFloorPlane = true;
	Params.FloorY = 0.0f;

	FHitResult Hit{};
	const bool bHit = Scene.SphereTraceSingleByChannel(
		Hit, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f), 0.35f, ECollisionChannel::Visibility, Params);
	TestTrue("Trace hit", bHit);
	TestTrue("Floor plane hit", Hit.bFloorPlane);
	TestEqual("Impact Y", Hit.ImpactPoint.Y, 0.0f, 1.0e-3f);
	TestTrue("Normal points up", Hit.ImpactNormal.Y > 0.5f);
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
	Scene.GetBodies()[Id].Position = FVector(0.0f, 1.0f, 0.0f);
	Scene.GetBodies()[Id].HalfExtents = FVector(1.0f, 1.0f, 1.0f); // top at y=2

	FHitResult Hit{};
	const float Radius = 0.35f;
	const float HalfHeight = 0.5f;
	const bool bHit = Scene.CapsuleTraceSingleByChannel(
		Hit, FVector(0.0f, 3.0f, 0.0f), FVector(0.0f, 1.5f, 0.0f), Radius, HalfHeight, ECollisionChannel::Visibility);
	TestTrue("Trace hit", bHit);
	TestTrue("Blocking hit", Hit.bBlockingHit);
	TestTrue("Impact at or below the top", Hit.ImpactPoint.Y <= 2.0f + 1.0e-2f);
	TestTrue("Normal points up", Hit.ImpactNormal.Y > 0.5f);
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
	Scene.GetBodies()[NearId].Position = FVector(0.0f, 0.5f, 0.0f);
	Scene.GetBodies()[NearId].HalfExtents = FVector(0.5f, 0.5f, 0.5f);

	const int32 FarId = Scene.AddBody({1, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[FarId].Position = FVector(0.0f, 0.5f, 3.0f);
	Scene.GetBodies()[FarId].HalfExtents = FVector(0.5f, 0.5f, 0.5f);

	FCollisionQueryParams Params{};
	Params.bTraceFloorPlane = false;

	TArray<FHitResult> Hits;
	const bool bMultiHit = Scene.LineTraceMultiByChannel(
		Hits, FVector(0.0f, 0.5f, -2.0f), FVector(0.0f, 0.5f, 5.0f), ECollisionChannel::WorldStatic, Params);
	TestTrue("Multi trace hit", bMultiHit);
	if (!TestEqual("Hit count", Hits.Num(), 2))
	{
		return false;
	}
	TestTrue("Sorted by time", Hits[0].Time < Hits[1].Time);
	TestEqual("Near mesh index", Hits[0].LevelMeshIndex, static_cast<SIZE_T>(0));
	TestEqual("Far mesh index", Hits[1].LevelMeshIndex, static_cast<SIZE_T>(1));

	FHitResult Single{};
	const bool bSingleHit = Scene.LineTraceSingleByChannel(
		Single, FVector(0.0f, 0.5f, -2.0f), FVector(0.0f, 0.5f, 5.0f), ECollisionChannel::WorldStatic, Params);
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
	Scene.GetBodies()[Id].Position = FVector(0.0f, 2.0f, 0.0f);
	Scene.GetBodies()[Id].HalfExtents = FVector(1.0f, 0.5f, 1.0f); // top at 2.5, bottom at 1.5

	FCollisionQueryParams Params{};
	Params.bTraceFloorPlane = true;
	Params.FloorY = 0.0f;

	TArray<FHitResult> Hits;
	const bool bMultiHit = Scene.SphereTraceMultiByChannel(
		Hits, FVector(0.0f, 4.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f), 0.25f, ECollisionChannel::Visibility, Params);
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
	Scene.GetBodies()[A].Position = FVector(0.0f, 1.0f, 0.0f);
	Scene.GetBodies()[A].HalfExtents = FVector(0.5f, 0.5f, 0.5f);
	const int32 B = Scene.AddBody({1, EBodyType::Dynamic, 1.0f, true});
	Scene.GetBodies()[B].Position = FVector(0.0f, 3.0f, 0.0f);
	Scene.GetBodies()[B].HalfExtents = FVector(0.5f, 0.5f, 0.5f);

	TArray<FHitResult> Hits;
	const bool bMultiHit = Scene.CapsuleTraceMultiByChannel(
		Hits, FVector(0.0f, 5.0f, 0.0f), FVector::ZeroVector, 0.2f, 0.3f, ECollisionChannel::Visibility);
	TestTrue("Multi trace hit", bMultiHit);
	if (!TestEqual("Hit count", Hits.Num(), 2))
	{
		return false;
	}
	TestTrue("Sorted by time", Hits[0].Time < Hits[1].Time);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
