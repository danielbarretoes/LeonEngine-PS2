#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/GarbageCollection.h"
#include "UObject/WeakObjectPtrTemplates.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameFrameworkGameModeSpawnsGameAndPlayerStatesTest,
	"System.Engine.GameFramework.GameModeSpawnsGameAndPlayerStates",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameFrameworkGameModeSpawnsGameAndPlayerStatesTest::RunTest(const FString& Parameters)
{
	// The world spawns the game mode, the game mode its game state; a player controller spawns its player state, which
	// PostLogin adds to the game state and destroying the controller removes.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AGameModeBase* GameMode = World.SetGameMode(AGameModeBase::StaticClass());
	if (!TestNotNull("Game mode", GameMode))
	{
		return false;
	}
	TestTrue("World keeps the game mode", World.GetAuthGameMode() == GameMode);
	TestTrue("Set once", World.SetGameMode(AGameMode::StaticClass()) == GameMode);
	TestTrue("Game state spawned", World.GetGameState() == &GameMode->GetGameState());
	TestTrue("Game state is an actor of the level", World.PersistentLevel->Actors.Contains(World.GetGameState()));

	APlayerController* Player = World.SpawnActor<APlayerController>();
	APlayerState* PlayerState = Player->GetPlayerState<APlayerState>();
	if (!TestNotNull("Player state spawned", PlayerState))
	{
		return false;
	}
	TestTrue("Owned by the controller", PlayerState->GetOwner() == Player);
	GameMode->PostLogin(*Player);
	TestEqual("One player", GameMode->GetNumPlayers(), 1);
	TestTrue("In the player array", GameMode->GetGameState().HasPlayerState(PlayerState));
	GameMode->Logout(*Player);
	TestEqual("No players", GameMode->GetNumPlayers(), 0);

	TWeakObjectPtr<APlayerState> WeakPlayerState = PlayerState;
	Player->Destroy();
	TestTrue("Player state destroyed with its controller", PlayerState->IsPendingKillPending());
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse("Player state collected", WeakPlayerState.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameFrameworkGameModeMatchStateTest, "System.Engine.GameFramework.GameModeMatchState",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameFrameworkGameModeMatchStateTest::RunTest(const FString& Parameters)
{
	// AGameMode walks the UE match states and mirrors them in its AGameState, whose clock runs while in progress.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AGameMode* GameMode = Cast<AGameMode>(World.SetGameMode(AGameMode::StaticClass()));
	if (!TestNotNull("Game mode", GameMode))
	{
		return false;
	}
	AGameState* GameState = GameMode->GetGameState<AGameState>();
	if (!TestNotNull("AGameState", GameState))
	{
		return false;
	}
	TestEqual("Waiting once the world plays", GameMode->GetMatchState(), MatchState::WaitingToStart);
	TestFalse("Not started", GameMode->HasMatchStarted());

	GameMode->StartMatch();
	TestEqual("In progress", GameMode->GetMatchState(), MatchState::InProgress);
	TestEqual("Mirrored", GameState->GetMatchState(), MatchState::InProgress);
	TestEqual("Previous state", GameState->GetPreviousMatchState(), MatchState::WaitingToStart);
	TestTrue("Base clock started", GameState->HasMatchStarted());
	GameState->Tick(1.5f);
	GameState->Tick(1.0f);
	TestEqual("Whole seconds", GameState->ElapsedTime, 2);
	TestEqual("Base clock", GameState->GetServerWorldTimeSeconds(), 2.5f, 1.0e-5f);

	GameMode->EndMatch();
	TestEqual("Waiting post match", GameMode->GetMatchState(), MatchState::WaitingPostMatch);
	TestTrue("Ended", GameMode->HasMatchEnded());
	TestTrue("Base clock ended", GameState->HasMatchEnded());
	GameMode->AbortMatch();
	TestEqual("Aborted", GameMode->GetMatchState(), MatchState::Aborted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameFrameworkControllersAreActorsTest,
	"System.Engine.GameFramework.ControllersAreActors",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameFrameworkControllersAreActorsTest::RunTest(const FString& Parameters)
{
	// Controllers live in the world like pawns; a destroyed pawn is released and the collector clears it everywhere.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	APlayerController* Player = World.SpawnActor<APlayerController>();
	ACharacter* Character = World.SpawnActor<ACharacter>();
	TestTrue("Controller in the level", World.PersistentLevel->Actors.Contains(Player));
	Player->Possess(Character);
	TestTrue("Possessed", Character->GetController() == Player);
	TestTrue("Controller's character", Player->GetCharacter() == Character);

	Character->Destroy();
	TestNull("Released on destroy", Player->GetPawn());
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestNull("Still released after the collection", Player->GetPawn());
	TestEqual("Controller and its player state", World.ActorCount(), static_cast<SIZE_T>(2));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
