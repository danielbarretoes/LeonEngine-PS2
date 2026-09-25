#include "AI/Navigation/NavigationSystem.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Physics/PhysScene.h"
#include "Tests/LegacyGolden.h"
#include "TriangleCollision.h"

#if WITH_DEV_AUTOMATION_TESTS

// Collision query and navigation goldens (Arcade physics scene), recorded in the legacy world before P7.

namespace
{

	/** Trace and path tolerance, metres. */
	constexpr float GoldenQueryPositionTolerance = 1.0e-3f;
	constexpr float GoldenQueryNormalTolerance = 1.0e-4f;
	constexpr float GoldenQueryTimeTolerance = 1.0e-4f;

	/** Adds a static box body from a legacy centre and legacy half extents; returns its body index. */
	int32 AddGoldenQueryBox(FPhysScene& Scene, const FVector& LegacyCenter, const FVector& LegacyHalfExtents)
	{
		const int32 Id = Scene.AddBody({static_cast<SIZE_T>(Scene.GetBodies().Num()), EBodyType::Static, 1.0f, true});
		FBodyInstance& Body = Scene.GetBodies()[Id];
		Body.Position = LegacyGolden::ToWorldPosition(LegacyCenter);
		Body.HalfExtents = LegacyGolden::ToWorldExtent(LegacyHalfExtents);
		return Id;
	}

	/**
	 * The fixed trace scene: a box, a triangle-mesh ramp, a 30 degree slope plane, and (through the query params) the
	 * floor plane at y = 0.
	 */
	void BuildGoldenTraceScene(FPhysScene& Scene)
	{
		// Box: x [1.5, 2.5], y [0, 1], z [-0.5, 0.5].
		AddGoldenQueryBox(Scene, FVector(2.0f, 0.5f, 0.0f), FVector(0.5f, 0.5f, 0.5f));

		// Triangle-mesh ramp rising along +X from y = 0.2 (x = -3) to y = 1 (x = -1), z in [-1, 1].
		const int32 RampId = AddGoldenQueryBox(Scene, FVector(-2.0f, 0.6f, 0.0f), FVector(1.0f, 0.4f, 1.0f));
		Scene.GetBodies()[RampId].CollisionShape = EBodyCollisionShape::TriangleMesh;
		FTriangleMeshCollision Ramp;
		Ramp.Positions = {LegacyGolden::ToWorldPosition(FVector(-3.0f, 0.2f, -1.0f)),
			LegacyGolden::ToWorldPosition(FVector(-1.0f, 1.0f, -1.0f)),
			LegacyGolden::ToWorldPosition(FVector(-1.0f, 1.0f, 1.0f)),
			LegacyGolden::ToWorldPosition(FVector(-3.0f, 0.2f, 1.0f))};
		Ramp.Indices = {0, 1, 2, 0, 2, 3};
		Scene.GetTriangleMeshes()[RampId] = MoveTemp(Ramp);

		// Slope plane through (0, 0, 4), clipped by a 3 m box.
		Scene.AddSlopeRamp(LegacyGolden::ToWorldPosition(FVector(0.0f, 0.0f, 4.0f)),
			LegacyGolden::ToWorldExtent(FVector(1.5f, 1.5f, 1.5f)), 30.0f);
	}

	FCollisionQueryParams GoldenTraceParams()
	{
		FCollisionQueryParams Params;
		Params.bTraceFloorPlane = true;
		Params.FloorY = LegacyGolden::ToWorldLength(0.0f);
		return Params;
	}

	/** The recorded fields of a set of single traces. */
	struct FGoldenTraceResults
	{
		/** Per trace: bBlockingHit, bFloorPlane. */
		TArray<bool> Flags;
		TArray<float> Times;
		/** Zero for a miss, so defaults of FHitResult are not part of the golden. */
		TArray<FVector> ImpactPoints;
		TArray<FVector> Locations;
		TArray<FVector> ImpactNormals;

		void Add(bool bHit, const FHitResult& Hit)
		{
			Flags.Add(bHit);
			Flags.Add(bHit && Hit.bFloorPlane);
			Times.Add(bHit ? Hit.Time : 1.0f);
			ImpactPoints.Add(bHit ? Hit.ImpactPoint : FVector::ZeroVector);
			Locations.Add(bHit ? Hit.Location : FVector::ZeroVector);
			ImpactNormals.Add(bHit ? Hit.ImpactNormal : FVector::ZeroVector);
		}
	};

	/** A trace segment in the legacy world. */
	struct FGoldenTraceSegment
	{
		FVector Start;
		FVector End;
	};

	/** The traces of the three trace goldens: box top, ramp, slope, floor only, box side and a miss. */
	constexpr FGoldenTraceSegment GoldenTraceSegments[] = {
		{FVector(2.2f, 3.0f, 0.2f), FVector(2.2f, -1.0f, 0.2f)},
		{FVector(-2.0f, 3.0f, 0.3f), FVector(-2.1f, -1.0f, 0.3f)},
		{FVector(0.2f, 3.0f, 4.1f), FVector(0.2f, -1.0f, 4.1f)},
		{FVector(0.5f, 3.0f, -2.0f), FVector(0.7f, -1.0f, -2.5f)},
		{FVector(-0.5f, 0.6f, 0.1f), FVector(4.0f, 0.6f, 0.1f)},
		{FVector(5.0f, 3.0f, 5.0f), FVector(6.0f, 2.0f, 6.0f)},
	};
	constexpr int32 GoldenTraceCount = UE_ARRAY_COUNT(GoldenTraceSegments);

	/** Checks the recorded fields of a trace golden against its tables. */
	void CheckGoldenTraceResults(FAutomationTestBase& Test, const FGoldenTraceResults& Results,
		const bool (&ExpectedFlags)[GoldenTraceCount * 2], const float (&ExpectedTimes)[GoldenTraceCount],
		const FVector (&ExpectedImpactPoints)[GoldenTraceCount], const FVector (&ExpectedLocations)[GoldenTraceCount],
		const FVector (&ExpectedImpactNormals)[GoldenTraceCount])
	{
		LegacyGolden::CheckBools(Test, "Flags", Results.Flags, ExpectedFlags, GoldenTraceCount * 2);
		LegacyGolden::CheckScalars(Test, "Times", Results.Times, ExpectedTimes, GoldenTraceCount,
			GoldenQueryTimeTolerance, LegacyGolden::EUnit::Unitless);
		LegacyGolden::CheckPositions(Test, "ImpactPoints", Results.ImpactPoints, ExpectedImpactPoints, GoldenTraceCount,
			GoldenQueryPositionTolerance);
		LegacyGolden::CheckPositions(
			Test, "Locations", Results.Locations, ExpectedLocations, GoldenTraceCount, GoldenQueryPositionTolerance);
		LegacyGolden::CheckDirections(Test, "ImpactNormals", Results.ImpactNormals, ExpectedImpactNormals,
			GoldenTraceCount, GoldenQueryNormalTolerance);
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenLineTracesTest, "System.Engine.Golden.LineTraces",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenLineTracesTest::RunTest(const FString& Parameters)
{
	// Line traces over the fixed scene: hit flags, times, impact points, locations and normals.
	FPhysScene Scene;
	BuildGoldenTraceScene(Scene);
	FGoldenTraceResults Results;
	for (const FGoldenTraceSegment& Segment : GoldenTraceSegments)
	{
		FHitResult Hit{};
		const bool bHit = Scene.LineTraceSingleByChannel(Hit, LegacyGolden::ToWorldPosition(Segment.Start),
			LegacyGolden::ToWorldPosition(Segment.End), ECollisionChannel::Visibility, GoldenTraceParams());
		Results.Add(bHit, Hit);
	}

	static const bool ExpectedFlags[GoldenTraceCount * 2] = {
		true, false, true, false, true, false, true, true, true, false, false, false};
	static const float ExpectedTimes[GoldenTraceCount] = {0.5f, 0.606060505f, 0.721132517f, 0.75f, 0.444444448f, 1.0f};
	static const FVector ExpectedImpactPoints[GoldenTraceCount] = {FVector(2.20000005f, 1.0f, 0.200000003f),
		FVector(-2.060606f, 0.57575798f, 0.300000012f), FVector(0.200000003f, 0.115469933f, 4.0999999f),
		FVector(0.649999976f, 0.0f, -2.375f), FVector(1.5f, 0.600000024f, 0.100000001f), FVector(0.0f, 0.0f, 0.0f)};
	static const FVector ExpectedLocations[GoldenTraceCount] = {FVector(2.20000005f, 1.0f, 0.200000003f),
		FVector(-2.060606f, 0.57575798f, 0.300000012f), FVector(0.200000003f, 0.115469933f, 4.0999999f),
		FVector(0.649999976f, 0.0f, -2.375f), FVector(1.5f, 0.600000024f, 0.100000001f), FVector(0.0f, 0.0f, 0.0f)};
	static const FVector ExpectedImpactNormals[GoldenTraceCount] = {FVector(0.0f, 1.0f, 0.0f),
		FVector(-0.3713907f, 0.928476751f, -0.0f), FVector(-0.5f, 0.866025388f, 0.0f), FVector(0.0f, 1.0f, 0.0f),
		FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f)};
	CheckGoldenTraceResults(
		*this, Results, ExpectedFlags, ExpectedTimes, ExpectedImpactPoints, ExpectedLocations, ExpectedImpactNormals);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenSphereTracesTest, "System.Engine.Golden.SphereTraces",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenSphereTracesTest::RunTest(const FString& Parameters)
{
	// Sphere traces (radius 0.3 m) over the fixed scene: hit flags, times, impact points, locations and normals.
	FPhysScene Scene;
	BuildGoldenTraceScene(Scene);
	FGoldenTraceResults Results;
	for (const FGoldenTraceSegment& Segment : GoldenTraceSegments)
	{
		FHitResult Hit{};
		const bool bHit = Scene.SphereTraceSingleByChannel(Hit, LegacyGolden::ToWorldPosition(Segment.Start),
			LegacyGolden::ToWorldPosition(Segment.End), LegacyGolden::ToWorldLength(0.3f),
			ECollisionChannel::Visibility, GoldenTraceParams());
		Results.Add(bHit, Hit);
	}

	static const bool ExpectedFlags[GoldenTraceCount * 2] = {
		true, false, true, false, true, false, true, true, true, false, false, false};
	static const float ExpectedTimes[GoldenTraceCount] = {
		0.425000012f, 0.524467111f, 0.634529948f, 0.675000012f, 0.377777785f, 1.0f};
	static const FVector ExpectedImpactPoints[GoldenTraceCount] = {FVector(2.20000005f, 0.99999994f, 0.200000003f),
		FVector(-1.94102943f, 0.623588562f, 0.300000012f), FVector(0.350000024f, 0.202072591f, 4.0999999f),
		FVector(0.63499999f, -5.96046448e-08f, -2.3375001f), FVector(1.5f, 0.600000024f, 0.100000001f),
		FVector(0.0f, 0.0f, 0.0f)};
	static const FVector ExpectedLocations[GoldenTraceCount] = {FVector(2.20000005f, 1.29999995f, 0.200000003f),
		FVector(-2.0524466f, 0.902131557f, 0.300000012f), FVector(0.200000003f, 0.461880207f, 4.0999999f),
		FVector(0.63499999f, 0.299999952f, -2.3375001f), FVector(1.20000005f, 0.600000024f, 0.100000001f),
		FVector(0.0f, 0.0f, 0.0f)};
	static const FVector ExpectedImpactNormals[GoldenTraceCount] = {FVector(0.0f, 1.0f, 0.0f),
		FVector(-0.3713907f, 0.928476751f, -0.0f), FVector(-0.5f, 0.866025388f, 0.0f), FVector(0.0f, 1.0f, 0.0f),
		FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f)};
	CheckGoldenTraceResults(
		*this, Results, ExpectedFlags, ExpectedTimes, ExpectedImpactPoints, ExpectedLocations, ExpectedImpactNormals);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenCapsuleTracesTest, "System.Engine.Golden.CapsuleTraces",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenCapsuleTracesTest::RunTest(const FString& Parameters)
{
	// Capsule traces (radius 0.25 m, half height 0.5 m) over the fixed scene: the same fields as the line traces.
	FPhysScene Scene;
	BuildGoldenTraceScene(Scene);
	FGoldenTraceResults Results;
	for (const FGoldenTraceSegment& Segment : GoldenTraceSegments)
	{
		FHitResult Hit{};
		const bool bHit = Scene.CapsuleTraceSingleByChannel(Hit, LegacyGolden::ToWorldPosition(Segment.Start),
			LegacyGolden::ToWorldPosition(Segment.End), LegacyGolden::ToWorldLength(0.25f),
			LegacyGolden::ToWorldLength(0.5f), ECollisionChannel::Visibility, GoldenTraceParams());
		Results.Add(bHit, Hit);
	}

	static const bool ExpectedFlags[GoldenTraceCount * 2] = {
		true, false, true, false, true, false, true, true, true, false, false, false};
	static const float ExpectedTimes[GoldenTraceCount] = {
		0.3125f, 0.402077079f, 0.52396369f, 0.5625f, 0.388888896f, 1.0f};
	static const FVector ExpectedImpactPoints[GoldenTraceCount] = {FVector(2.20000005f, 1.0f, 0.200000003f),
		FVector(-1.76166463f, 0.695334136f, 0.300000012f), FVector(0.54150635f, 0.312638879f, 4.0999999f),
		FVector(0.612500012f, 0.0f, -2.28125f), FVector(1.5f, 0.600000024f, 0.100000001f), FVector(0.0f, 0.0f, 0.0f)};
	static const FVector ExpectedLocations[GoldenTraceCount] = {FVector(2.20000005f, 1.75f, 0.200000003f),
		FVector(-2.04020762f, 1.39169168f, 0.300000012f), FVector(0.200000003f, 0.904145241f, 4.0999999f),
		FVector(0.612500012f, 0.75f, -2.28125f), FVector(1.25f, 0.600000024f, 0.100000001f), FVector(0.0f, 0.0f, 0.0f)};
	static const FVector ExpectedImpactNormals[GoldenTraceCount] = {FVector(0.0f, 1.0f, 0.0f),
		FVector(-0.3713907f, 0.928476751f, -0.0f), FVector(-0.5f, 0.866025388f, 0.0f), FVector(0.0f, 1.0f, 0.0f),
		FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f)};
	CheckGoldenTraceResults(
		*this, Results, ExpectedFlags, ExpectedTimes, ExpectedImpactPoints, ExpectedLocations, ExpectedImpactNormals);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenNavigationFindPathTest, "System.Engine.Golden.NavigationFindPath",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenNavigationFindPathTest::RunTest(const FString& Parameters)
{
	// A NavMesh baked over a floor slab and a pillar: grid size and origin, a hash of the walkable mask, and the
	// waypoints of a path that detours around the pillar.
	FPhysScene Physics;
	AddGoldenQueryBox(Physics, FVector(0.0f, 0.0f, 0.0f), FVector(20.0f, 0.5f, 20.0f));
	AddGoldenQueryBox(Physics, FVector(0.25f, 1.0f, -0.25f), FVector(0.6f, 1.5f, 0.8f));

	UNavigationSystem Nav;
	Nav.SetCellSize(LegacyGolden::ToWorldLength(0.5f));
	Nav.SetAgentRadius(LegacyGolden::ToWorldLength(0.35f));
	Nav.BuildFromPhysScene(Physics, LegacyGolden::ToWorldLength(0.0f), LegacyGolden::ToWorldLength(6.0f));
	if (!TestTrue("Nav mesh built", Nav.HasNavMesh()))
	{
		return false;
	}
	const FNavMesh& Mesh = Nav.GetNavMesh();
	const TArray<int32> Counts = {Mesh.Width, Mesh.Depth, Nav.GetWalkableCellCount(), Nav.GetBlockerCount()};
	const TArray<FVector> Origin = {FVector(Mesh.OriginX, Mesh.FloorY, Mesh.OriginZ)};
	const uint32 MaskHash = FCrc::MemCrc32(Mesh.Walkable.GetData(), Mesh.Walkable.Num());

	TArray<FVector> Path;
	const bool bFound = Nav.FindPath(LegacyGolden::ToWorldPosition(FVector(-4.0f, 0.0f, -0.2f)),
		LegacyGolden::ToWorldPosition(FVector(4.0f, 0.0f, 0.3f)), Path);

	static const int32 ExpectedCounts[4] = {24, 24, 551, 1};
	static const FVector ExpectedOrigin[1] = {FVector(-6.0f, 0.0f, -6.0f)};
	constexpr uint32 ExpectedMaskHash = 0x3888ADF8u;
	static const bool ExpectedFound[1] = {true};
	static const FVector ExpectedPath[] = {FVector(-3.75f, 0.0f, -0.25f), FVector(-3.25f, 0.0f, -0.25f),
		FVector(-2.75f, 0.0f, -0.25f), FVector(-2.25f, 0.0f, 0.25f), FVector(-1.75f, 0.0f, 0.75f),
		FVector(-1.25f, 0.0f, 1.25f), FVector(-0.75f, 0.0f, 1.25f), FVector(-0.25f, 0.0f, 1.25f),
		FVector(0.25f, 0.0f, 1.25f), FVector(0.75f, 0.0f, 1.25f), FVector(1.25f, 0.0f, 1.25f),
		FVector(1.75f, 0.0f, 1.25f), FVector(2.25f, 0.0f, 1.25f), FVector(2.75f, 0.0f, 0.75f),
		FVector(3.25f, 0.0f, 0.75f), FVector(3.75f, 0.0f, 0.25f), FVector(4.25f, 0.0f, 0.25f)};
	LegacyGolden::CheckInts(*this, "Counts", Counts, ExpectedCounts, 4);
	LegacyGolden::CheckPositions(*this, "Origin", Origin, ExpectedOrigin, 1, GoldenQueryPositionTolerance);
	LegacyGolden::CheckHash(*this, "MaskHash", MaskHash, ExpectedMaskHash);
	LegacyGolden::CheckBools(*this, "Found", TArray<bool>{bFound}, ExpectedFound, 1);
	LegacyGolden::CheckPositions(
		*this, "Path", Path, ExpectedPath, UE_ARRAY_COUNT(ExpectedPath), GoldenQueryPositionTolerance);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
