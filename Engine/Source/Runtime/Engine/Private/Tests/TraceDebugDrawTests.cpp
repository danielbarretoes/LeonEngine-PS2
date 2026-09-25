#include "CollisionQuery.h"
#include "CoreMinimal.h"
#include "Debug/DebugDraw.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTraceDebugDrawLineTraceMissAndHitFillDebugDrawTest,
	"System.Engine.TraceDebugDraw.DrawDebugLineTraceMissAndHitFillDebugDraw",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTraceDebugDrawLineTraceMissAndHitFillDebugDrawTest::RunTest(const FString& Parameters)
{
	// DrawDebugLineTrace draws both a missed trace and a trace with a blocking hit.
	FDebugDraw Draw;

	DrawDebugLineTrace(Draw, FVector::ZeroVector, FVector(0.0f, 0.0f, 100.0f), TArray<FHitResult>());
	TestFalse("Miss drawn", Draw.IsEmpty());

	Draw.Clear();
	FHitResult Hit{};
	Hit.bBlockingHit = true;
	Hit.Time = 0.5f;
	Hit.ImpactPoint = FVector(0.0f, 0.0f, 50.0f);
	Hit.ImpactNormal = FVector(0.0f, 0.0f, 1.0f);
	TArray<FHitResult> Hits;
	Hits.Add(Hit);
	DrawDebugLineTrace(Draw, FVector::ZeroVector, FVector(0.0f, 0.0f, 100.0f), Hits);
	TestFalse("Hit drawn", Draw.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTraceDebugDrawLineTraceForOneFrameDrawsViaPhysSceneTest,
	"System.Engine.TraceDebugDraw.LineTraceForOneFrameDrawsViaPhysScene",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTraceDebugDrawLineTraceForOneFrameDrawsViaPhysSceneTest::RunTest(const FString& Parameters)
{
	// With DrawDebugType ForOneFrame the scene traces draw into the given FDebugDraw, on a hit and on a miss.
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[Id].Position = FVector(0.0f, 0.0f, 50.0f);
	Scene.GetBodies()[Id].HalfExtents = FVector(50.0f, 50.0f, 50.0f);

	FDebugDraw Draw;
	FCollisionQueryParams Params{};
	Params.DrawDebugType = EDrawDebugTrace::ForOneFrame;

	FHitResult Hit{};
	const bool bHit = Scene.LineTraceSingleByChannel(Hit, FVector(0.0f, -200.0f, 50.0f), FVector(0.0f, 200.0f, 50.0f),
		ECollisionChannel::WorldStatic, Params, &Draw);
	TestTrue("Trace hit", bHit);
	TestFalse("Hit drawn", Draw.IsEmpty());

	Draw.Clear();
	TArray<FHitResult> Misses;
	const bool bMissHit = Scene.LineTraceMultiByChannel(Misses, FVector(1000.0f, -200.0f, 50.0f),
		FVector(1000.0f, 200.0f, 50.0f), ECollisionChannel::WorldStatic, Params, &Draw);
	TestFalse("Trace missed", bMissHit);
	TestFalse("Miss drawn", Draw.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTraceDebugDrawSphereTraceAndCapsuleTraceFillBatchTest,
	"System.Engine.TraceDebugDraw.DrawDebugSphereTraceAndCapsuleTraceFillBatch",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTraceDebugDrawSphereTraceAndCapsuleTraceFillBatchTest::RunTest(const FString& Parameters)
{
	// The sphere and capsule trace helpers draw even without hits.
	FDebugDraw Draw;
	DrawDebugSphereTrace(Draw, FVector(0.0f, 0.0f, 100.0f), FVector::ZeroVector, 35.0f, TArray<FHitResult>());
	TestFalse("Sphere trace drawn", Draw.IsEmpty());

	Draw.Clear();
	DrawDebugCapsuleTrace(Draw, FVector(0.0f, 0.0f, 100.0f), FVector::ZeroVector, 30.0f, 50.0f, TArray<FHitResult>());
	TestFalse("Capsule trace drawn", Draw.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
