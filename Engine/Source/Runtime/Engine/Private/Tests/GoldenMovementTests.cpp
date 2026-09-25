#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "Tests/LegacyGolden.h"

#if WITH_DEV_AUTOMATION_TESTS

// Character movement goldens (Arcade physics scene), recorded in the legacy world before P7.
// The movement tunables keep their engine defaults, which are tuned in metres (MaxWalkSpeed 4.5, Gravity 24,
// JumpZVelocity 7, MaxStepHeight 0.35, WalkableFloorZ 0.71, WalkBounds 18, capsule 0.35 x 0.925): the P7 commits
// convert the defaults, not these tests.

namespace
{

	constexpr float GoldenMovementDeltaTime = 1.0f / 60.0f;
	/** Trajectory tolerance, metres. */
	constexpr float GoldenMovementPositionTolerance = 1.0e-3f;
	/** Tolerance of the final velocities, metres per second. */
	constexpr float GoldenMovementSpeedTolerance = 1.0e-2f;

	/** Adds a static box body from a legacy centre and legacy half extents. */
	void AddGoldenMovementBox(FPhysScene& Scene, const FVector& LegacyCenter, const FVector& LegacyHalfExtents)
	{
		const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
		FBodyInstance& Body = Scene.GetBodies()[Id];
		Body.Position = LegacyGolden::ToWorldPosition(LegacyCenter);
		Body.HalfExtents = LegacyGolden::ToWorldExtent(LegacyHalfExtents);
	}

	/** Spawns a walking character with its feet at a legacy location and the floor plane at a legacy height. */
	ACharacter* SpawnGoldenCharacter(UWorld& World, const FVector& LegacyFeet, float LegacyFloorY)
	{
		ACharacter* Character = World.SpawnActor<ACharacter>();
		Character->GetCharacterMovement().FloorY = LegacyGolden::ToWorldLength(LegacyFloorY);
		Character->Reset(LegacyGolden::ToWorldPosition(LegacyFeet), 0.0f);
		return Character;
	}

	/** What one simulated run of a character produced. */
	struct FGoldenMovementRun
	{
		/** Feet location every SampleEvery frames (engine world). */
		TArray<FVector> Samples;
		/** Movement mode after every frame. */
		TArray<bool> GroundedPerFrame;
		/** Velocity over the last frame (engine world). */
		FVector LastVelocity = FVector::ZeroVector;
	};

	/**
	 * Runs Frames movement frames at 60 Hz with a constant legacy wish direction (zero for no input) and samples the
	 * feet every SampleEvery frames.
	 */
	FGoldenMovementRun RunGoldenMovement(
		ACharacter& Character, FPhysScene& Scene, const FVector& LegacyWish, int32 Frames, int32 SampleEvery)
	{
		FGoldenMovementRun Run;
		for (int32 Frame = 1; Frame <= Frames; ++Frame)
		{
			const FVector Before = Character.GetActorLocation();
			if (!LegacyWish.IsZero())
			{
				Character.AddMovementInput(LegacyGolden::ToWorldDirection(LegacyWish));
			}
			Character.PerformMovement(Scene, GoldenMovementDeltaTime, nullptr);
			Run.GroundedPerFrame.Add(Character.IsMovingOnGround());
			Run.LastVelocity = (Character.GetActorLocation() - Before) / GoldenMovementDeltaTime;
			if (Frame % SampleEvery == 0)
			{
				Run.Samples.Add(Character.GetActorLocation());
			}
		}
		return Run;
	}

	/** First frame (1-based) whose grounded state equals bGrounded, or 0 when none does. */
	int32 FirstGoldenFrame(const TArray<bool>& GroundedPerFrame, bool bGrounded)
	{
		for (int32 Index = 0; Index < GroundedPerFrame.Num(); ++Index)
		{
			if (GroundedPerFrame[Index] == bGrounded)
			{
				return Index + 1;
			}
		}
		return 0;
	}

	/** Checks the final state shared by the movement goldens: grounded / walkable floor and the velocities. */
	void CheckGoldenFinalState(FAutomationTestBase& Test, const ACharacter& Character, const FGoldenMovementRun& Run,
		const bool (&ExpectedFlags)[2], const float (&ExpectedVelocityZ)[1], const FVector (&ExpectedVelocity)[1])
	{
		const TArray<bool> Flags = {Character.IsMovingOnGround(), Character.GetCurrentFloor().bWalkableFloor};
		LegacyGolden::CheckBools(Test, "FinalFlags", Flags, ExpectedFlags, 2);
		LegacyGolden::CheckScalars(Test, "FinalVelocityZ", TArray<float>{Character.GetVelocityZ()}, ExpectedVelocityZ,
			1, GoldenMovementSpeedTolerance, LegacyGolden::EUnit::Speed);
		// A velocity converts like a position: the axes swap and metres become engine units.
		LegacyGolden::CheckPositions(Test, "FinalVelocity", TArray<FVector>{Run.LastVelocity}, ExpectedVelocity, 1,
			GoldenMovementSpeedTolerance);
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenWalkOnFlatGroundTest, "System.Engine.Golden.WalkOnFlatGround",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenWalkOnFlatGroundTest::RunTest(const FString& Parameters)
{
	// One second of walking on the floor plane along a diagonal wish: the path, then a grounded walk at full speed.
	UWorld World;
	ACharacter* Character = SpawnGoldenCharacter(World, FVector::ZeroVector, 0.0f);
	const FGoldenMovementRun Run =
		RunGoldenMovement(*Character, World.GetPhysicsScene(), FVector(1.0f, 0.0f, 0.5f), 60, 6);

	static const FVector ExpectedPath[] = {FVector(0.402492255f, 0.0f, 0.201246127f),
		FVector(0.80498451f, 0.0f, 0.402492255f), FVector(1.20747674f, 0.0f, 0.603738368f),
		FVector(1.60996902f, 0.0f, 0.80498451f), FVector(2.01246119f, 0.0f, 1.00623059f),
		FVector(2.41495275f, 0.0f, 1.20747638f), FVector(2.81744432f, 0.0f, 1.40872216f),
		FVector(3.21993589f, 0.0f, 1.60996795f), FVector(3.62242746f, 0.0f, 1.81121373f),
		FVector(4.02491903f, 0.0f, 2.01245952f)};
	static const bool ExpectedFinalFlags[2] = {true, true};
	static const float ExpectedFinalVelocityZ[1] = {0.0f};
	static const FVector ExpectedFinalVelocity[1] = {FVector(4.02491522f, 0.0f, 2.01245761f)};
	LegacyGolden::CheckPositions(
		*this, "Path", Run.Samples, ExpectedPath, UE_ARRAY_COUNT(ExpectedPath), GoldenMovementPositionTolerance);
	CheckGoldenFinalState(*this, *Character, Run, ExpectedFinalFlags, ExpectedFinalVelocityZ, ExpectedFinalVelocity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenSlideAlongWallTest, "System.Engine.Golden.SlideAlongWall",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenSlideAlongWallTest::RunTest(const FString& Parameters)
{
	// A diagonal walk into a long wall stops across the wall and slides along it.
	UWorld World;
	ACharacter* Character = SpawnGoldenCharacter(World, FVector(-1.5f, 0.0f, 0.0f), 0.0f);
	AddGoldenMovementBox(World.GetPhysicsScene(), FVector(0.0f, 1.0f, 0.0f), FVector(0.25f, 1.0f, 4.0f));
	const FGoldenMovementRun Run =
		RunGoldenMovement(*Character, World.GetPhysicsScene(), FVector(1.0f, 0.0f, 1.0f), 90, 9);

	static const FVector ExpectedPath[] = {FVector(-1.02270305f, 0.0f, 0.477297008f),
		FVector(-0.63414216f, 0.0f, 0.926309764f), FVector(-0.634142101f, 0.0f, 1.27632785f),
		FVector(-0.63414216f, 0.0f, 1.62634599f), FVector(-0.634142101f, 0.0f, 1.97636402f),
		FVector(-0.63414216f, 0.0f, 2.32638168f), FVector(-0.634142101f, 0.0f, 2.67639923f),
		FVector(-0.63414216f, 0.0f, 3.02641678f), FVector(-0.634142101f, 0.0f, 3.37643433f),
		FVector(-0.63414216f, 0.0f, 3.72645187f)};
	static const bool ExpectedFinalFlags[2] = {true, true};
	static const float ExpectedFinalVelocityZ[1] = {0.0f};
	static const FVector ExpectedFinalVelocity[1] = {FVector(-3.57627846e-06f, 0.0f, 2.33345008f)};
	LegacyGolden::CheckPositions(
		*this, "Path", Run.Samples, ExpectedPath, UE_ARRAY_COUNT(ExpectedPath), GoldenMovementPositionTolerance);
	CheckGoldenFinalState(*this, *Character, Run, ExpectedFinalFlags, ExpectedFinalVelocityZ, ExpectedFinalVelocity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenJumpArcTest, "System.Engine.Golden.JumpArc",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenJumpArcTest::RunTest(const FString& Parameters)
{
	// A jump from rest: the whole arc every 3 frames, the frame it lands on, and the rest state after landing.
	UWorld World;
	ACharacter* Character = SpawnGoldenCharacter(World, FVector(0.5f, 0.0f, -0.5f), 0.0f);
	FPhysScene& Scene = World.GetPhysicsScene();
	(void)RunGoldenMovement(*Character, Scene, FVector::ZeroVector, 10, 10);
	Character->Jump();
	const FGoldenMovementRun Run = RunGoldenMovement(*Character, Scene, FVector::ZeroVector, 45, 3);
	const TArray<int32> Frames = {
		FirstGoldenFrame(Run.GroundedPerFrame, false), FirstGoldenFrame(Run.GroundedPerFrame, true)};

	static const FVector ExpectedArc[] = {FVector(0.5f, 0.310000002f, -0.5f), FVector(0.5f, 0.560000002f, -0.5f),
		FVector(0.5f, 0.75f, -0.5f), FVector(0.5f, 0.879999936f, -0.5f), FVector(0.5f, 0.949999869f, -0.5f),
		FVector(0.5f, 0.959999859f, -0.5f), FVector(0.5f, 0.909999728f, -0.5f), FVector(0.5f, 0.799999654f, -0.5f),
		FVector(0.5f, 0.629999518f, -0.5f), FVector(0.5f, 0.39999938f, -0.5f), FVector(0.5f, 0.0f, -0.5f),
		FVector(0.5f, 0.0f, -0.5f), FVector(0.5f, 0.0f, -0.5f), FVector(0.5f, 0.0f, -0.5f), FVector(0.5f, 0.0f, -0.5f)};
	static const int32 ExpectedTakeOffAndLandingFrames[2] = {1, 31};
	static const bool ExpectedFinalFlags[2] = {true, true};
	static const float ExpectedFinalVelocityZ[1] = {0.0f};
	static const FVector ExpectedFinalVelocity[1] = {FVector(0.0f, 0.0f, 0.0f)};
	LegacyGolden::CheckPositions(
		*this, "Arc", Run.Samples, ExpectedArc, UE_ARRAY_COUNT(ExpectedArc), GoldenMovementPositionTolerance);
	LegacyGolden::CheckInts(*this, "TakeOffAndLandingFrames", Frames, ExpectedTakeOffAndLandingFrames, 2);
	CheckGoldenFinalState(*this, *Character, Run, ExpectedFinalFlags, ExpectedFinalVelocityZ, ExpectedFinalVelocity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenStepUpLedgeTest, "System.Engine.Golden.StepUpLedge",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenStepUpLedgeTest::RunTest(const FString& Parameters)
{
	// A 0.3 m ledge (below MaxStepHeight) is stepped onto and walked along.
	UWorld World;
	ACharacter* Character = SpawnGoldenCharacter(World, FVector(-1.5f, 0.0f, 0.2f), 0.0f);
	AddGoldenMovementBox(World.GetPhysicsScene(), FVector(8.0f, 0.15f, 0.0f), FVector(8.0f, 0.15f, 4.0f));
	const FGoldenMovementRun Run =
		RunGoldenMovement(*Character, World.GetPhysicsScene(), FVector(1.0f, 0.0f, 0.0f), 60, 6);

	static const FVector ExpectedPath[] = {FVector(-1.04999971f, 0.0f, 0.200000003f),
		FVector(-0.599999785f, 0.0f, 0.200000003f), FVector(-0.0649999827f, 0.299999982f, 0.200000003f),
		FVector(0.38500005f, 0.299999982f, 0.200000003f), FVector(0.834999979f, 0.299999982f, 0.200000003f),
		FVector(1.28500009f, 0.299999982f, 0.200000003f), FVector(1.73500037f, 0.299999982f, 0.200000003f),
		FVector(2.18500066f, 0.299999982f, 0.200000003f), FVector(2.63500094f, 0.299999982f, 0.200000003f),
		FVector(3.08500123f, 0.299999982f, 0.200000003f)};
	static const bool ExpectedFinalFlags[2] = {true, true};
	static const float ExpectedFinalVelocityZ[1] = {0.0f};
	static const FVector ExpectedFinalVelocity[1] = {FVector(4.50000238f, 0.0f, 0.0f)};
	LegacyGolden::CheckPositions(
		*this, "Path", Run.Samples, ExpectedPath, UE_ARRAY_COUNT(ExpectedPath), GoldenMovementPositionTolerance);
	CheckGoldenFinalState(*this, *Character, Run, ExpectedFinalFlags, ExpectedFinalVelocityZ, ExpectedFinalVelocity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenRefuseTallStepTest, "System.Engine.Golden.RefuseTallStep",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenRefuseTallStepTest::RunTest(const FString& Parameters)
{
	// A 0.5 m block (above MaxStepHeight) stops the character in front of it without a step up.
	UWorld World;
	ACharacter* Character = SpawnGoldenCharacter(World, FVector(-1.5f, 0.0f, -0.3f), 0.0f);
	AddGoldenMovementBox(World.GetPhysicsScene(), FVector(0.5f, 0.25f, 0.0f), FVector(0.5f, 0.25f, 4.0f));
	const FGoldenMovementRun Run =
		RunGoldenMovement(*Character, World.GetPhysicsScene(), FVector(1.0f, 0.0f, 0.0f), 60, 6);

	static const FVector ExpectedPath[] = {FVector(-1.04999971f, 0.0f, -0.300000012f),
		FVector(-0.599999785f, 0.0f, -0.300000012f), FVector(-0.389999986f, 0.0f, -0.300000012f),
		FVector(-0.389999986f, 0.0f, -0.300000012f), FVector(-0.389999986f, 0.0f, -0.300000012f),
		FVector(-0.389999986f, 0.0f, -0.300000012f), FVector(-0.389999986f, 0.0f, -0.300000012f),
		FVector(-0.389999986f, 0.0f, -0.300000012f), FVector(-0.389999986f, 0.0f, -0.300000012f),
		FVector(-0.389999986f, 0.0f, -0.300000012f)};
	static const bool ExpectedFinalFlags[2] = {true, true};
	static const float ExpectedFinalVelocityZ[1] = {0.0f};
	static const FVector ExpectedFinalVelocity[1] = {FVector(0.0f, 0.0f, 0.0f)};
	LegacyGolden::CheckPositions(
		*this, "Path", Run.Samples, ExpectedPath, UE_ARRAY_COUNT(ExpectedPath), GoldenMovementPositionTolerance);
	CheckGoldenFinalState(*this, *Character, Run, ExpectedFinalFlags, ExpectedFinalVelocityZ, ExpectedFinalVelocity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenWalkUpRampTest, "System.Engine.Golden.WalkUpRamp",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenWalkUpRampTest::RunTest(const FString& Parameters)
{
	// A 20 degree ramp is walkable: the character settles on it, then climbs it on a walkable floor.
	UWorld World;
	FPhysScene& Scene = World.GetPhysicsScene();
	Scene.AddSlopeRamp(LegacyGolden::ToWorldPosition(FVector::ZeroVector),
		LegacyGolden::ToWorldExtent(FVector(8.0f, 8.0f, 2.0f)), 20.0f);
	const float StartX = -1.5f;
	const float StartY = (StartX * FMath::Tan(FMath::DegreesToRadians(20.0f))) + 0.05f;
	ACharacter* Character = SpawnGoldenCharacter(World, FVector(StartX, StartY, 0.0f), -100.0f);
	(void)RunGoldenMovement(*Character, Scene, FVector::ZeroVector, 15, 15);
	const FGoldenMovementRun Run = RunGoldenMovement(*Character, Scene, FVector(1.0f, 0.0f, 0.0f), 60, 6);

	static const FVector ExpectedPath[] = {FVector(-1.04999971f, -0.338598818f, 0.0f),
		FVector(-0.599999785f, -0.174812227f, 0.0f), FVector(-0.149999827f, -0.011025697f, 0.0f),
		FVector(0.300000191f, 0.152760923f, 0.0f), FVector(0.750000119f, 0.316547513f, 0.0f),
		FVector(1.20000017f, 0.480334163f, 0.0f), FVector(1.65000045f, 0.644120812f, 0.0f),
		FVector(2.10000062f, 0.807907522f, 0.0f), FVector(2.55000091f, 0.971694171f, 0.0f),
		FVector(3.00000119f, 1.13548088f, 0.0f)};
	static const FVector ExpectedFloorNormal[1] = {FVector(-0.342020124f, 0.939692616f, 0.0f)};
	static const bool ExpectedFinalFlags[2] = {true, true};
	static const float ExpectedFinalVelocityZ[1] = {0.0f};
	static const FVector ExpectedFinalVelocity[1] = {FVector(4.50000238f, 1.63786399f, 0.0f)};
	LegacyGolden::CheckPositions(
		*this, "Path", Run.Samples, ExpectedPath, UE_ARRAY_COUNT(ExpectedPath), GoldenMovementPositionTolerance);
	LegacyGolden::CheckDirections(*this, "FloorNormal", TArray<FVector>{Character->GetCurrentFloor().Hit.ImpactNormal},
		ExpectedFloorNormal, 1, 1.0e-4f);
	CheckGoldenFinalState(*this, *Character, Run, ExpectedFinalFlags, ExpectedFinalVelocityZ, ExpectedFinalVelocity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenSteepSlopeKeepsFallingTest, "System.Engine.Golden.SteepSlopeKeepsFalling",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenSteepSlopeKeepsFallingTest::RunTest(const FString& Parameters)
{
	// A 55 degree slope is not walkable: a character dropped on it keeps falling and does not sink through it.
	UWorld World;
	FPhysScene& Scene = World.GetPhysicsScene();
	Scene.AddSlopeRamp(LegacyGolden::ToWorldPosition(FVector::ZeroVector),
		LegacyGolden::ToWorldExtent(FVector(4.0f, 4.0f, 2.0f)), 55.0f);
	ACharacter* Character = SpawnGoldenCharacter(World, FVector::ZeroVector, -100.0f);
	Character->ApplyReplicatedState(LegacyGolden::ToWorldPosition(FVector(0.5f, 2.0f, 0.25f)), 0.0f, 0.0f, false);
	const FGoldenMovementRun Run = RunGoldenMovement(*Character, Scene, FVector::ZeroVector, 60, 6);

	static const FVector ExpectedPath[] = {FVector(0.5f, 1.86000013f, 0.25f), FVector(0.5f, 1.4799999f, 0.25f),
		FVector(0.5f, 1.17999983f, 0.25f), FVector(0.5f, 1.13999987f, 0.25f), FVector(0.5f, 1.1235286f, 0.25f),
		FVector(0.5f, 1.1235286f, 0.25f), FVector(0.5f, 1.1235286f, 0.25f), FVector(0.5f, 1.1235286f, 0.25f),
		FVector(0.5f, 1.1235286f, 0.25f), FVector(0.5f, 1.1235286f, 0.25f)};
	static const bool ExpectedFinalFlags[2] = {false, false};
	static const float ExpectedFinalVelocityZ[1] = {0.0f};
	static const FVector ExpectedFinalVelocity[1] = {FVector(0.0f, 0.0f, 0.0f)};
	LegacyGolden::CheckPositions(
		*this, "Path", Run.Samples, ExpectedPath, UE_ARRAY_COUNT(ExpectedPath), GoldenMovementPositionTolerance);
	CheckGoldenFinalState(*this, *Character, Run, ExpectedFinalFlags, ExpectedFinalVelocityZ, ExpectedFinalVelocity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenWalkOffLedgeAndFallTest, "System.Engine.Golden.WalkOffLedgeAndFall",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenWalkOffLedgeAndFallTest::RunTest(const FString& Parameters)
{
	// Walking off a 1 m platform: the frame the character starts falling, the fall, and the landing on the floor.
	UWorld World;
	FPhysScene& Scene = World.GetPhysicsScene();
	AddGoldenMovementBox(Scene, FVector(0.0f, 0.5f, 0.0f), FVector(0.5f, 0.5f, 0.5f));
	ACharacter* Character = SpawnGoldenCharacter(World, FVector(0.0f, 1.0f, 0.1f), 0.0f);
	(void)RunGoldenMovement(*Character, Scene, FVector::ZeroVector, 10, 10);
	const FGoldenMovementRun Run = RunGoldenMovement(*Character, Scene, FVector(1.0f, 0.0f, 0.2f), 60, 4);
	const int32 FallFrame = FirstGoldenFrame(Run.GroundedPerFrame, false);
	int32 LandingFrame = 0;
	for (int32 Index = FallFrame; Index > 0 && Index < Run.GroundedPerFrame.Num(); ++Index)
	{
		if (Run.GroundedPerFrame[Index])
		{
			LandingFrame = Index + 1;
			break;
		}
	}

	static const FVector ExpectedPath[] = {FVector(0.294174224f, 1.0f, 0.158834845f),
		FVector(0.588348448f, 1.0f, 0.217669696f), FVector(0.882522643f, 0.99333334f, 0.276504517f),
		FVector(0.985483706f, 0.900000036f, 0.297096729f), FVector(1.08844471f, 0.699999988f, 0.317688942f),
		FVector(1.19140577f, 0.393333316f, 0.338281155f), FVector(1.43777668f, 0.0f, 0.387555301f),
		FVector(1.73195088f, 0.0f, 0.446390092f), FVector(2.02612519f, 0.0f, 0.505224884f),
		FVector(2.32029939f, 0.0f, 0.564059675f), FVector(2.61447358f, 0.0f, 0.622894466f),
		FVector(2.90864778f, 0.0f, 0.681729257f), FVector(3.20282197f, 0.0f, 0.740564048f),
		FVector(3.49699616f, 0.0f, 0.799398839f), FVector(3.79117036f, 0.0f, 0.858233631f)};
	static const int32 ExpectedFallAndLandingFrames[2] = {12, 25};
	static const bool ExpectedFinalFlags[2] = {true, true};
	static const float ExpectedFinalVelocityZ[1] = {0.0f};
	static const FVector ExpectedFinalVelocity[1] = {FVector(4.41261244f, 0.0f, 0.882521808f)};
	LegacyGolden::CheckPositions(
		*this, "Path", Run.Samples, ExpectedPath, UE_ARRAY_COUNT(ExpectedPath), GoldenMovementPositionTolerance);
	LegacyGolden::CheckInts(
		*this, "FallAndLandingFrames", TArray<int32>{FallFrame, LandingFrame}, ExpectedFallAndLandingFrames, 2);
	CheckGoldenFinalState(*this, *Character, Run, ExpectedFinalFlags, ExpectedFinalVelocityZ, ExpectedFinalVelocity);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
