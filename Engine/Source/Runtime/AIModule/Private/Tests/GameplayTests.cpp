#include "AI/Navigation/NavigationPath.h"
#include "AI/Navigation/NavigationSystem.h"
#include "AI/Navigation/NavigationWaypoint.h"
#include "AIController.h"
#include "BodyInstance.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "Debug/DebugDraw.h"
#include "Engine/BlockingVolume.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "PhysicsBackend.h"
#include "Tests/GameplayTestTypes.h"
#include "Tests/ScopedTestWorld.h"
#include "TriangleCollision.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayWorldSpawnsTicksAndDestroysActorsTest,
	"System.AIModule.Gameplay.WorldSpawnsTicksAndDestroysActors",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayWorldSpawnsTicksAndDestroysActorsTest::RunTest(const FString& Parameters)
{
	// A spawned Actor belongs to its World, is purged on the Tick after Destroy, and Clear empties the World.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ATestActor* Actor = World.SpawnActor<ATestActor>();
	if (!TestNotNull("Spawned actor", Actor))
	{
		return false;
	}
	TestEqual("One actor", World.ActorCount(), static_cast<SIZE_T>(1));
	TestTrue("Actor world", Actor->GetWorld() == &World);

	Actor->SetActorLocation(FVector(100.0f, 200.0f, 300.0f));
	TestEqual("Location Y", Actor->GetActorLocation().Y, 200.0f, 1.0e-3f);

	World.DestroyActor(Actor);
	TestTrue("Pending kill", Actor->IsPendingKillPending());
	World.Tick(0.016f);
	TestEqual("Purged after tick", World.ActorCount(), static_cast<SIZE_T>(0));

	World.SpawnActor<ATestActor>();
	World.Clear();
	TestEqual("Empty after clear", World.ActorCount(), static_cast<SIZE_T>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayWorldFindFirstFindsDerivedTypeTest,
	"System.AIModule.Gameplay.WorldFindFirstFindsDerivedType",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayWorldFindFirstFindsDerivedTypeTest::RunTest(const FString& Parameters)
{
	// FindFirst returns the first Actor of the requested type and null when none matches.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	World.SpawnActor<ATestActor>();
	ATestPawn* Pawn = World.SpawnActor<ATestPawn>();
	TestTrue("Finds the pawn", World.FindFirst<ATestPawn>() == Pawn);
	TestNull("No character", World.FindFirst<ACharacter>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayControllerPossessAndUnPossessTest,
	"System.AIModule.Gameplay.ControllerPossessAndUnPossess",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayControllerPossessAndUnPossessTest::RunTest(const FString& Parameters)
{
	// Possess links the Controller and the Pawn both ways; UnPossess clears both links.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ATestPawn* Pawn = World.SpawnActor<ATestPawn>();
	ATestController& Controller = *World.SpawnActor<ATestController>();
	Controller.Possess(Pawn);
	TestTrue("Controller has pawn", Controller.HasPawn());
	TestTrue("Pawn possessed", Pawn->IsPossessed());
	TestTrue("Pawn controller", Pawn->GetController() == &Controller);

	Controller.UnPossess();
	TestFalse("Controller has no pawn", Controller.HasPawn());
	TestFalse("Pawn not possessed", Pawn->IsPossessed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayPawnDestroyUnPossessesControllerTest,
	"System.AIModule.Gameplay.PawnDestroyUnPossessesController",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayPawnDestroyUnPossessesControllerTest::RunTest(const FString& Parameters)
{
	// Destroying a possessed Pawn releases its Controller.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ATestPawn* Pawn = World.SpawnActor<ATestPawn>();
	ATestController& Controller = *World.SpawnActor<ATestController>();
	Controller.Possess(Pawn);
	Pawn->Destroy();
	World.Tick(0.0f);
	TestFalse("Controller released", Controller.HasPawn());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayGameStateMatchTimerAndPlayerStateScoreTest,
	"System.AIModule.Gameplay.GameStateMatchTimerAndPlayerStateScore",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayGameStateMatchTimerAndPlayerStateScoreTest::RunTest(const FString& Parameters)
{
	// The match clock runs only while the match is in progress; Reset clears the clock and the player score.
	AGameStateBase& GameState = *NewObject<AGameStateBase>();
	GameState.HandleMatchHasStarted();
	GameState.Tick(0.5f);
	TestTrue("Match started", GameState.HasMatchStarted());
	TestEqual("Clock advanced", GameState.GetServerWorldTimeSeconds(), 0.5f, 1.0e-5f);
	GameState.Reset();
	TestEqual("Clock reset", GameState.GetServerWorldTimeSeconds(), 0.0f, 1.0e-5f);
	TestFalse("Match reset", GameState.HasMatchStarted());

	APlayerState& PlayerState = *NewObject<APlayerState>();
	PlayerState.SetPlayerId(2);
	PlayerState.SetPlayerName("P2");
	PlayerState.AddScore(10.0f);
	TestEqual("Player id", PlayerState.GetPlayerId(), 2);
	TestEqual("Player name", PlayerState.GetPlayerName(), "P2");
	TestEqual("Score", PlayerState.GetScore(), 10.0f, 1.0e-5f);
	PlayerState.Reset();
	TestEqual("Score reset", PlayerState.GetScore(), 0.0f, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayGameInstanceNotifyLevelOpenedTest,
	"System.AIModule.Gameplay.GameInstanceNotifyLevelOpened",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayGameInstanceNotifyLevelOpenedTest::RunTest(const FString& Parameters)
{
	// Each NotifyLevelOpened bumps the opened-level counter.
	UGameInstance& GameInstance = *NewObject<UGameInstance>();
	TestEqual("No levels yet", GameInstance.GetLevelsOpened(), 0);
	GameInstance.NotifyLevelOpened();
	GameInstance.NotifyLevelOpened();
	TestEqual("Two levels", GameInstance.GetLevelsOpened(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplaySpringArmClampsPitchAndArmLengthTest,
	"System.AIModule.Gameplay.SpringArmComponentClampsPitchAndArmLength",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplaySpringArmClampsPitchAndArmLengthTest::RunTest(const FString& Parameters)
{
	// With bUsePawnControlRotation the arm follows the pawn's control rotation, whose pitch the player controller
	// clamps; the arm length input clamps to its limits, and the arm drives an orbit camera at the target offset
	// height.
	FScopedTestWorld TestWorld;
	ATestPawn& Pawn = *TestWorld->SpawnActor<ATestPawn>();
	APlayerController& Controller = *TestWorld->SpawnActor<APlayerController>();
	Controller.Possess(&Pawn);
	USpringArmComponent* Arm = NewObject<USpringArmComponent>(&Pawn);
	Arm->bUsePawnControlRotation = true;

	Pawn.AddControllerPitchInput(200.0f);
	TestEqual("Pitch clamped to max", Pawn.GetControlRotation().Pitch, Controller.ViewPitchMax, 1.0e-4f);
	Pawn.AddControllerPitchInput(-400.0f);
	TestEqual("Pitch clamped to min", Pawn.GetControlRotation().Pitch, Controller.ViewPitchMin, 1.0e-4f);
	Pawn.AddControllerPitchInput(60.0f);
	Pawn.AddControllerYawInput(30.0f);
	TestTrue("Arm follows the control rotation",
		Arm->GetTargetRotation().Equals(FRotator(Controller.ViewPitchMin + 60.0f, 30.0f, 0.0f), 1.0e-4f));

	Arm->AddArmLengthInput(10000.0f);
	TestEqual("Arm length clamped to max", Arm->TargetArmLength, Arm->ArmLengthMax, 1.0e-3f);

	Arm->SnapLagState(FVector::ZeroVector);
	UCameraComponent& Camera = *NewObject<UCameraComponent>();
	Arm->ApplyToCamera(Camera, FVector(100.0f, 0.0f, 0.0f), 0.016f);
	TestTrue("Orbit camera", Camera.GetMode() == ECameraMode::Orbit);
	TestEqual("Camera target height", Camera.GetTarget().Z, Arm->TargetOffset.Z, 50.0f);
	TestTrue("Camera looks along the control rotation",
		Camera.GetViewRotation().Equals(Controller.GetControlRotation(), 1.0e-4f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayPawnLookInputDrivesControlRotationTest,
	"System.AIModule.Gameplay.PawnLookInputDrivesControlRotation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayPawnLookInputDrivesControlRotationTest::RunTest(const FString& Parameters)
{
	// AddControllerYawInput / AddControllerPitchInput reach only a possessing player controller: without one the pawn
	// has no control rotation and views along its actor rotation.
	FScopedTestWorld TestWorld;
	ATestPawn& Pawn = *TestWorld->SpawnActor<ATestPawn>();
	Pawn.SetActorRotation(FRotator(0.0f, 45.0f, 0.0f));
	Pawn.AddControllerYawInput(10.0f);
	TestTrue("No control rotation unpossessed", Pawn.GetControlRotation().Equals(FRotator::ZeroRotator, 0.0f));
	TestTrue("Views along the actor unpossessed", Pawn.GetViewRotation().Equals(FRotator(0.0f, 45.0f, 0.0f), 0.0f));

	{
		ATestController& Controller = *TestWorld->SpawnActor<ATestController>();
		Controller.Possess(&Pawn);
		Pawn.AddControllerYawInput(10.0f);
		TestTrue("Not a player controller", Controller.GetControlRotation().Equals(FRotator::ZeroRotator, 0.0f));
		Controller.Destroy();
	}

	APlayerController& Player = *TestWorld->SpawnActor<APlayerController>();
	Player.Possess(&Pawn);
	Player.SetControlRotation(FRotator(-10.0f, 90.0f, 0.0f));
	Pawn.AddControllerYawInput(15.0f);
	Pawn.AddControllerPitchInput(25.0f);
	TestTrue("Look input adds to the control rotation",
		Player.GetControlRotation().Equals(FRotator(15.0f, 105.0f, 0.0f), 1.0e-4f));
	TestTrue("Pawn view is the control rotation", Pawn.GetViewRotation().Equals(Player.GetControlRotation(), 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplaySpringArmCollisionProbeShortensArmTest,
	"System.AIModule.Gameplay.SpringArmComponentCollisionProbeShortensArm",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplaySpringArmCollisionProbeShortensArmTest::RunTest(const FString& Parameters)
{
	// A static box between the pawn and the camera pulls the arm in, but never below ArmLengthMin.
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[Id].Position = FVector(200.0f, 0.0f, 100.0f);
	Scene.GetBodies()[Id].HalfExtents = FVector(25.0f, 200.0f, 100.0f);

	// Without a pawn the arm uses its own rotation: the view looks toward -X, so the camera sits along +X.
	USpringArmComponent& Arm = *NewObject<USpringArmComponent>();
	Arm.bDoCollisionTest = true;
	Arm.bEnableCameraLag = false;
	Arm.bEnableCameraRotationLag = false;
	Arm.ArmLengthLagSpeed = 1000.0f;
	Arm.TargetArmLength = 600.0f;
	Arm.ArmLengthMin = 50.0f;
	Arm.RelativeRotation = FRotator(0.0f, 180.0f, 0.0f);
	Arm.TargetOffset = FVector(0.0f, 0.0f, 100.0f);
	Arm.SocketOffset = FVector::ZeroVector;
	Arm.ProbeSize = 15.0f;
	Arm.CollisionProbeOffset = 5.0f;
	Arm.SnapLagState(FVector::ZeroVector);

	UCameraComponent& Camera = *NewObject<UCameraComponent>();
	Arm.ApplyToCamera(Camera, FVector::ZeroVector, 0.016f, &Scene);
	TestTrue("Arm shortened", Camera.GetDistance() < 300.0f);
	TestTrue("Arm above minimum", Camera.GetDistance() >= Arm.ArmLengthMin);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayAIControllerSteersTowardTargetAndArrivesTest,
	"System.AIModule.Gameplay.AIControllerSteersTowardTargetAndArrives",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayAIControllerSteersTowardTargetAndArrivesTest::RunTest(const FString& Parameters)
{
	// Far from the target the wish is a unit vector toward it; inside the arrive radius it is zero.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(0.0f, 0.0f, 0.0f));

	AAIController& Ai = *World.SpawnActor<AAIController>();
	Ai.Possess(Character);
	Ai.SetArriveRadius(50.0f);
	Ai.MoveToLocation(FVector(1000.0f, 0.0f, 0.0f));

	const FVector WishFar = Ai.TickAI(0.016f);
	TestEqual("Unit wish when far", WishFar.Size(), 1.0f, 1.0e-3f);
	TestTrue("Wish points at target", WishFar.X > 0.5f);

	Character->Reset(FVector(1000.0f, 0.0f, 0.0f));
	const FVector WishNear = Ai.TickAI(0.016f);
	TestEqual("No wish on arrival", WishNear.Size(), 0.0f, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayAIControllerMoveToActorTracksMovingTargetTest,
	"System.AIModule.Gameplay.AIControllerMoveToActorTracksMovingTarget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayAIControllerMoveToActorTracksMovingTargetTest::RunTest(const FString& Parameters)
{
	// MoveToActor follows the target Actor's current location and stops once it is within the arrive radius.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Hunter = World.SpawnActor<ACharacter>();
	ACharacter* Prey = World.SpawnActor<ACharacter>();
	Hunter->Reset(FVector(0.0f, 0.0f, 0.0f));
	Prey->Reset(FVector(800.0f, 0.0f, 0.0f));

	AAIController& Ai = *World.SpawnActor<AAIController>();
	Ai.Possess(Hunter);
	Ai.SetArriveRadius(40.0f);
	Ai.MoveToActor(Prey);

	const FVector Wish = Ai.TickAI(0.016f);
	TestTrue("Wish points at prey", Wish.X > 0.5f);
	TestTrue("Move actor is prey", Ai.GetMoveActor() == Prey);

	Prey->Reset(FVector(20.0f, 0.0f, 0.0f));
	const FVector WishArrived = Ai.TickAI(0.016f);
	TestEqual("No wish on arrival", WishArrived.Size(), 0.0f, 1.0e-5f);
	return true;
}

namespace
{

	/** A floor slab 40 m square with its top at Z = 0 (a blocking box). */
	void SpawnNavFloor(UWorld& World)
	{
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -10.0f), FVector(40.0f, 40.0f, 0.2f)));
	}

	/** A blocking box of Size cm with its bottom on the floor, centred on X / Y. */
	void SpawnNavBox(UWorld& World, float X, float Y, const FVector& Size)
	{
		(void)World.SpawnActor<ABlockingVolume>(
			ABlockingVolume::StaticClass(), FTransform(FQuat::Identity, FVector(X, Y, Size.Z * 0.5f), Size / 100.0f));
	}

	/** A waypoint 50 cm above Floor (as the maps place them). */
	ANavigationWaypoint* SpawnWaypoint(UWorld& World, const FVector& Floor)
	{
		return World.SpawnActor<ANavigationWaypoint>(Floor + FVector(0.0f, 0.0f, 50.0f), FRotator::ZeroRotator);
	}

	/** The wall scene: a 3 m tall wall 10 m long across X = 0, waypoints west, north of the wall's end and east. */
	void SpawnWallScene(UWorld& World, TArray<ANavigationWaypoint*>& OutWaypoints)
	{
		SpawnNavFloor(World);
		SpawnNavBox(World, 0.0f, 0.0f, FVector(120.0f, 1000.0f, 300.0f));
		OutWaypoints.Add(SpawnWaypoint(World, FVector(-500.0f, 0.0f, 0.0f)));
		OutWaypoints.Add(SpawnWaypoint(World, FVector(-500.0f, 700.0f, 0.0f)));
		OutWaypoints.Add(SpawnWaypoint(World, FVector(500.0f, 700.0f, 0.0f)));
		OutWaypoints.Add(SpawnWaypoint(World, FVector(500.0f, 0.0f, 0.0f)));
		(void)UNavigationSystem::AutoLinkWaypoints(World);
		World.GetNavigationSystem().Build(World);
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayAIControllerPathFollowDoesNotShortcutTest,
	"System.AIModule.Gameplay.AIControllerPathFollowDoesNotShortcutThroughBlocker",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayAIControllerPathFollowDoesNotShortcutTest::RunTest(const FString& Parameters)
{
	// With the world's waypoint graph and a wall in the way, the controller follows the detour around the wall's end
	// instead of charging the wall, and gets to the goal.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	TArray<ANavigationWaypoint*> Waypoints;
	SpawnWallScene(World, Waypoints);
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(-500.0f, 0.0f, 0.0f));
	Character->GetCharacterMovement().WalkBounds = 100000.0f;

	AAIController& Ai = *World.SpawnActor<AAIController>();
	Ai.Possess(Character);
	// A large goal arrive radius must not skip the detour's points through the wall.
	Ai.SetArriveRadius(125.0f);
	Ai.MoveToLocation(FVector(500.0f, 0.0f, 0.0f));
	if (!TestTrue("Following a path", Ai.IsFollowingPath()))
	{
		return false;
	}
	TestTrue("Path has a detour", Ai.PathPoints().Num() >= 3);

	const FVector Wish = Ai.TickAI(0.016f);
	TestTrue("Moving", Wish.Size() > 0.5f);
	TestTrue("Steering around the wall", Wish.Y > 0.9f);
	for (int32 Frame = 0;
		 Frame < 600 && FVector::Dist2D(Character->GetActorLocation(), FVector(500.0f, 0.0f, 0.0f)) > 130.0f; ++Frame)
	{
		(void)Ai.TickAI(1.0f / 60.0f);
		World.Tick(1.0f / 60.0f);
	}
	TestTrue("Arrived around the wall",
		FVector::Dist2D(Character->GetActorLocation(), FVector(500.0f, 0.0f, 0.0f)) <= 130.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayNavFindPathRoutesAroundStaticBlockerTest,
	"System.AIModule.Gameplay.NavigationSystemFindPathRoutesAroundStaticBlocker",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayNavFindPathRoutesAroundStaticBlockerTest::RunTest(const FString& Parameters)
{
	// The auto-linked graph goes around the wall (no link through it), FindPath follows it, a point in the open goes
	// straight, FindPathToLocationSynchronously gives the same path from the start, and a point projects onto the
	// nearest waypoint's floor.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	TArray<ANavigationWaypoint*> Waypoints;
	SpawnWallScene(World, Waypoints);
	const UNavigationSystem& Nav = World.GetNavigationSystem();
	TestEqual("Four waypoints", Nav.GetNodes().Num(), 4);
	TestFalse("No link through the wall", Waypoints[0]->Links.Contains(Waypoints[3]));
	TestTrue("West to north", Waypoints[0]->Links.Contains(Waypoints[1]) && Waypoints[1]->Links.Contains(Waypoints[0]));
	TestTrue("Past the wall's end", Waypoints[1]->Links.Contains(Waypoints[2]));

	TArray<FVector> Path;
	if (!TestTrue("Path found", Nav.FindPath(FVector(-600.0f, 0.0f, 0.0f), FVector(600.0f, 0.0f, 0.0f), Path)))
	{
		return false;
	}
	TestTrue("Around the wall", Path.Num() >= 3 && Path[0].Y > 600.0f);
	TestTrue("The end last", Path.Last().Equals(FVector(600.0f, 0.0f, 0.0f)));
	TArray<FVector> Straight;
	TestTrue("In the open", Nav.FindPath(FVector(-800.0f, 0.0f, 0.0f), FVector(-800.0f, 600.0f, 0.0f), Straight));
	TestEqual("Straight to the end", Straight.Num(), 1);

	const UNavigationPath* NavPath = UNavigationSystem::FindPathToLocationSynchronously(
		&World, FVector(-600.0f, 0.0f, 0.0f), FVector(600.0f, 0.0f, 0.0f));
	TestTrue(
		"A UNavigationPath", NavPath != nullptr && NavPath->IsValid() && NavPath->PathPoints.Num() == Path.Num() + 1);
	TestTrue("Longer than the straight line", NavPath != nullptr && NavPath->GetPathLength() > 1400.0f);

	FVector Projected = FVector::ZeroVector;
	if (!TestTrue("Point projected", Nav.ProjectPointToNavigation(FVector(-600.0f, 0.0f, 200.0f), Projected)))
	{
		return false;
	}
	TestEqual("Projected to the floor", Projected.Z, 0.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayNavAutoLinkStepsJumpsAndDropsTest,
	"System.AIModule.Gameplay.NavigationAutoLinkStepsJumpsAndDrops",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayNavAutoLinkStepsJumpsAndDropsTest::RunTest(const FString& Parameters)
{
	// AutoLinkWaypoints: a 30 cm step links both ways, a 1.1 m crate is a jump up and a drop down, a 2.5 m ledge is a
	// drop only, a gap in the floor breaks a link, and waypoints beyond MaxLinkDistance stay apart.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	// Two floors with a 2 m gap between them (X from -100 to 100).
	(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
		FTransform(FQuat::Identity, FVector(-1100.0f, 0.0f, -10.0f), FVector(20.0f, 40.0f, 0.2f)));
	(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
		FTransform(FQuat::Identity, FVector(1100.0f, 0.0f, -10.0f), FVector(20.0f, 40.0f, 0.2f)));
	SpawnNavBox(World, -1000.0f, 0.0f, FVector(200.0f, 200.0f, 30.0f));
	SpawnNavBox(World, -1000.0f, 600.0f, FVector(200.0f, 200.0f, 110.0f));
	SpawnNavBox(World, -1000.0f, -600.0f, FVector(200.0f, 200.0f, 250.0f));
	ANavigationWaypoint* Ground = SpawnWaypoint(World, FVector(-1400.0f, 0.0f, 0.0f));
	ANavigationWaypoint* Step = SpawnWaypoint(World, FVector(-1000.0f, 0.0f, 30.0f));
	ANavigationWaypoint* Crate = SpawnWaypoint(World, FVector(-1000.0f, 600.0f, 110.0f));
	ANavigationWaypoint* Crate2 = SpawnWaypoint(World, FVector(-1400.0f, 600.0f, 0.0f));
	ANavigationWaypoint* Ledge = SpawnWaypoint(World, FVector(-1000.0f, -600.0f, 250.0f));
	ANavigationWaypoint* Below = SpawnWaypoint(World, FVector(-1400.0f, -600.0f, 0.0f));
	ANavigationWaypoint* AcrossGap = SpawnWaypoint(World, FVector(300.0f, 0.0f, 0.0f));
	ANavigationWaypoint* Far = SpawnWaypoint(World, FVector(1700.0f, 0.0f, 0.0f));
	FWaypointLinkParams Params;
	Params.MaxLinkDistance = 1500.0f;
	TestTrue("Links added", UNavigationSystem::AutoLinkWaypoints(World, Params) > 0);
	TestTrue("A step: both ways", Ground->Links.Contains(Step) && Step->Links.Contains(Ground));
	TestTrue("A crate: jump up and drop down", Crate2->Links.Contains(Crate) && Crate->Links.Contains(Crate2));
	TestTrue("A ledge: drop only", Ledge->Links.Contains(Below) && !Below->Links.Contains(Ledge));
	TestFalse("A gap breaks the way", Step->Links.Contains(AcrossGap) || AcrossGap->Links.Contains(Step));
	TestTrue("Within reach on the far floor", AcrossGap->Links.Contains(Far) && Far->Links.Contains(AcrossGap));
	TestFalse("Beyond MaxLinkDistance", Ground->Links.Contains(Far) || Far->Links.Contains(Ground));

	// A* over the links: shortest, and nothing across the gap.
	World.GetNavigationSystem().Build(World);
	const UNavigationSystem& Nav = World.GetNavigationSystem();
	TArray<int32> NodePath;
	TestTrue("Crate to below the ledge",
		UNavigationSystem::FindNodePath(Nav.GetNodes(), Nav.FindNode(Crate), Nav.FindNode(Below), NodePath));
	TestTrue("Through the ground",
		NodePath.Num() >= 2 && NodePath[0] == Nav.FindNode(Crate) && NodePath.Last() == Nav.FindNode(Below));
	TestFalse("No way over the gap",
		UNavigationSystem::FindNodePath(Nav.GetNodes(), Nav.FindNode(Ground), Nav.FindNode(AcrossGap), NodePath));
	TestTrue("Cleared on failure", NodePath.Num() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayNavAppendDebugDrawFillsOverlayTest,
	"System.AIModule.Gameplay.NavigationSystemAppendDebugDrawFillsOverlay",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayNavAppendDebugDrawFillsOverlayTest::RunTest(const FString& Parameters)
{
	// Drawing the waypoint graph adds its boxes and arrows to an empty debug draw batch.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	TArray<ANavigationWaypoint*> Waypoints;
	SpawnWallScene(World, Waypoints);
	FDebugDraw Draw;
	TestTrue("Starts empty", Draw.IsEmpty());
	World.GetNavigationSystem().AppendDebugDraw(Draw);
	TestFalse("Filled", Draw.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayCharacterResetJumpAndPerformMovementTest,
	"System.AIModule.Gameplay.CharacterResetJumpAndPerformMovement",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayCharacterResetJumpAndPerformMovementTest::RunTest(const FString& Parameters)
{
	// Reset puts the Character on the ground; Jump makes it fall upward, and movement input moves it along X.
	FScopedTestWorld TestWorld;
	ACharacter& Character = *TestWorld->SpawnActor<ACharacter>();
	Character.Reset(FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 45.0f, 0.0f));
	TestTrue("On ground after reset", Character.IsMovingOnGround());
	TestEqual("Yaw after reset", Character.GetActorRotation().Yaw, 45.0f, 1.0e-5f);

	FPhysScene Scene;
	Character.Jump();
	Character.PerformMovement(Scene, 1.0f / 60.0f);
	TestFalse("Left the ground", Character.IsMovingOnGround());
	TestTrue("Falling", Character.IsFalling());
	TestTrue("Moved up", Character.GetActorLocation().Z > 0.0f);

	Character.AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
	const float StartX = Character.GetActorLocation().X;
	Character.PerformMovement(Scene, 1.0f / 60.0f);
	TestTrue("Moved along X", Character.GetActorLocation().X > StartX);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayActorMeshComponentFollowsActorWithLegacyContentYawTest,
	"System.AIModule.Gameplay.ActorMeshComponentFollowsActorWithLegacyContentYaw",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayActorMeshComponentFollowsActorWithLegacyContentYawTest::RunTest(const FString& Parameters)
{
	// A mesh component attached to the actor's root follows the actor's location and yaw; it shows converted legacy
	// content (facing +Y), so its relative yaw is LegacyContentYaw and its world yaw is the actor yaw plus that.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ATestActor* Actor = World.SpawnActor<ATestActor>();
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor);
	Mesh->SetupAttachment(Actor->GetRootComponent());
	Mesh->RelativeRotation = FRotator(0.0f, LegacyContentYaw, 0.0f);
	Mesh->RegisterComponent();
	Actor->SetActorLocationAndRotation(FVector(300.0f, 150.0f, -200.0f), FRotator(0.0f, 90.0f, 0.0f));

	const FTransform Transform = Mesh->GetComponentTransform();
	TestEqual("Position X", Transform.GetLocation().X, 300.0f, 1.0e-3f);
	TestEqual("Position Y", Transform.GetLocation().Y, 150.0f, 1.0e-3f);
	TestEqual("Position Z", Transform.GetLocation().Z, -200.0f, 1.0e-3f);
	TestTrue(
		"Yaw", Transform.GetRotation().Equals(FRotator(0.0f, 90.0f + LegacyContentYaw, 0.0f).Quaternion(), 1.0e-6f));
	// The content's forward (+Y) now faces the actor's forward (yaw 90: +Y).
	TestTrue("Content faces the actor forward",
		Transform.GetRotation().RotateVector(FVector(0.0f, 1.0f, 0.0f)).Equals(FVector(0.0f, 1.0f, 0.0f), 1.0e-5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayWorldTickGameplayFrameMovesCharacterMeshTest,
	"System.AIModule.Gameplay.WorldTickGameplayFrameMovesCharacterMesh",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayWorldTickGameplayFrameMovesCharacterMeshTest::RunTest(const FString& Parameters)
{
	// After a gameplay frame the Character's mesh component shows the Character pose.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(100.0f, 200.0f, 0.0f), FRotator(0.0f, 45.0f, 0.0f));

	FWorldGameplayFrameParams Frame{};
	Frame.DeltaTime = 1.0f / 60.0f;
	World.TickGameplayFrame(Frame);

	const FTransform Transform = Character->GetMesh().GetComponentTransform();
	TestEqual("Position X", Transform.GetLocation().X, 100.0f, 1.0e-2f);
	TestEqual("Position Y", Transform.GetLocation().Y, 200.0f, 1.0e-2f);
	TestEqual("Yaw", Transform.Rotator().Yaw, 45.0f + LegacyContentYaw, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayActorComponentRegisterAndSubobjectTickTest,
	"System.AIModule.Gameplay.ActorComponentRegisterComponentAndCreateDefaultSubobjectTick",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayActorComponentRegisterAndSubobjectTickTest::RunTest(const FString& Parameters)
{
	// A component created after the spawn is registered on its owner, begins play with it, ticks when enabled and
	// stops once destroyed.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ATestActor* Actor = World.SpawnActor<ATestActor>();
	TestTrue("Root registered", Actor->GetComponents().Num() >= 1);

	UCountingComponent* Heap = NewObject<UCountingComponent>(Actor);
	if (!TestNotNull("Subobject created", Heap))
	{
		return false;
	}
	Heap->RegisterComponent();
	TestTrue("Owned by actor", Heap->GetOwner() == Actor);
	TestTrue("Registered", Heap->IsRegistered());
	Heap->SetComponentTickEnabled(true);

	// Registered on an actor that already plays: the component begins play at once.
	TestEqual("Began play once", Heap->Begins, 1);

	World.Tick(1.0f / 60.0f);
	TestEqual("Ticked once", Heap->Ticks, 1);

	Heap->DestroyComponent();
	TestFalse("Unregistered", Heap->IsRegistered());
	World.Tick(1.0f / 60.0f);
	TestEqual("No tick after unregister", Heap->Ticks, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplaySceneComponentAttachHierarchyTest,
	"System.AIModule.Gameplay.SceneComponentAttachHierarchyWorldTransform",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplaySceneComponentAttachHierarchyTest::RunTest(const FString& Parameters)
{
	// Attached components add their relative offsets to the Actor pose; cycles are refused and destroy detaches.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ATestActor* Actor = World.SpawnActor<ATestActor>();
	Actor->SetActorLocationAndRotation(FVector(1000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);

	// The Relative* fields are world units (cm).
	USceneComponent& Child = *NewObject<USceneComponent>(Actor);
	Child.RelativeLocation = FVector(200.0f, 0.0f, 0.0f);
	TestTrue("Child attached",
		Child.AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform));
	TestTrue("Child parent is root", Child.GetAttachParent() == Actor->GetRootComponent());
	TestEqual("Root has one child", Actor->GetRootComponent()->GetAttachChildren().Num(), 1);

	const FVector Loc = Child.GetComponentLocation();
	TestEqual("Child world X", Loc.X, 1200.0f, 1.0e-2f);

	USceneComponent& Grandchild = *NewObject<USceneComponent>(Actor);
	Grandchild.RelativeLocation = FVector(100.0f, 0.0f, 0.0f);
	TestTrue(
		"Grandchild attached", Grandchild.AttachToComponent(&Child, FAttachmentTransformRules::KeepRelativeTransform));
	TestEqual("Grandchild world X", Grandchild.GetComponentLocation().X, 1300.0f, 1.0e-2f);

	TestFalse("Cycle refused", Child.AttachToComponent(&Grandchild, FAttachmentTransformRules::KeepRelativeTransform));
	Child.DestroyComponent();
	TestNull("Child detached", Child.GetAttachParent());
	TestEqual("Root has no children", Actor->GetRootComponent()->GetAttachChildren().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayCharacterMeshAttachesToRootTest,
	"System.AIModule.Gameplay.CharacterMeshAttachesToRootSceneComponent",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayCharacterMeshAttachesToRootTest::RunTest(const FString& Parameters)
{
	// The Character mesh is a registered child of the root and follows the Actor location plus its offset.
	FScopedTestWorld TestWorld;
	ACharacter& Character = *TestWorld->SpawnActor<ACharacter>();
	TestTrue("Mesh parent is root", Character.GetMesh().GetAttachParent() == Character.GetRootComponent());
	TestTrue("Mesh owner", Character.GetMesh().GetOwner() == &Character);
	TestTrue("Mesh registered", Character.GetMesh().IsRegistered());
	TestTrue("Root registered", Character.GetRootComponent()->IsRegistered());
	Character.SetActorLocation(FVector(500.0f, 0.0f, 0.0f));
	// A relative offset of 100 cm, in the actor's space.
	Character.GetMesh().RelativeLocation = FVector(100.0f, 0.0f, 0.0f);
	TestEqual("Mesh world X", Character.GetMesh().GetComponentLocation().X, 600.0f, 1.0e-2f);

	// The mesh shows legacy content (facing +Y) with a relative yaw of -90: the content faces the actor's forward.
	Character.SetActorRotation(FRotator(0.0f, 30.0f, 0.0f));
	const FVector ContentForward =
		Character.GetMesh().GetComponentTransform().GetRotation().RotateVector(FVector(0.0f, 1.0f, 0.0f));
	TestTrue("Content faces the actor forward", ContentForward.Equals(FRotator(0.0f, 30.0f, 0.0f).Vector(), 1.0e-5f));
	TestTrue("Offset turns with the actor",
		Character.GetMesh().GetComponentLocation().Equals(
			FVector(500.0f, 0.0f, 0.0f) + FRotator(0.0f, 30.0f, 0.0f).RotateVector(FVector(100.0f, 0.0f, 0.0f)),
			1.0e-2f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayPhysSceneReportsArcadeBackendByDefaultTest,
	"System.AIModule.Gameplay.PhysSceneReportsArcadeBackendByDefault",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayPhysSceneReportsArcadeBackendByDefaultTest::RunTest(const FString& Parameters)
{
	// A default physics scene uses the Arcade backend and names it "Arcade".
	FPhysScene Scene;
	TestTrue("Arcade backend", Scene.GetBackend() == EPhysicsBackend::Arcade);
	TestEqual("Backend name", PhysicsBackendName(Scene.GetBackend()), "Arcade");
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
