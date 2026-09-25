#include "Components/ProgressBar.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameState.h"
#include "GameFramework/HUD.h"
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
	GameMode->PostLogin(*Player);

	TWeakObjectPtr<APlayerState> WeakPlayerState = PlayerState;
	Player->Destroy();
	TestTrue("Player state destroyed with its controller", PlayerState->IsPendingKillPending());
	TestEqual("Left the player array", GameMode->GetNumPlayers(), 0);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameFrameworkHUDWidgetsAreObjectsTest,
	"System.Engine.GameFramework.HUDWidgetsAreObjects",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameFrameworkHUDWidgetsAreObjectsTest::RunTest(const FString& Parameters)
{
	// HUD widgets are UObjects inside their HUD, found by class with Cast; a removed widget is collected.
	AHUD* Hud = NewObject<AHUD>();
	Hud->AddToRoot();
	UTextBlock* Text = Hud->AddWidget<UTextBlock>();
	TestTrue("Widget inside the HUD", Text->GetOuter() == Hud);
	TestTrue("Owning HUD", Text->GetOwningHUD() == Hud);
	TestTrue("Found by class", Hud->GetWidgetOfClass<UTextBlock>() == Text);
	TestNull("Other classes are not", Hud->GetWidgetOfClass<UProgressBar>());

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue("Kept by the HUD", Hud->GetWidgetOfClass<UTextBlock>() == Text);
	TWeakObjectPtr<UTextBlock> WeakText = Text;
	TestTrue("Removed by class", Hud->RemoveWidget<UTextBlock>());
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse("Collected once removed", WeakText.IsValid());
	Hud->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameFrameworkAnimInstanceIsAnInnerObjectTest,
	"System.Engine.GameFramework.AnimInstanceIsAnInnerObject",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameFrameworkAnimInstanceIsAnInnerObjectTest::RunTest(const FString& Parameters)
{
	// A skeletal mesh component starts with a locomotion anim instance inside it; the character instance replaces it
	// and Cast tells them apart.
	FScopedTestWorld TestWorld;
	ACharacter* Character = TestWorld->SpawnActor<ACharacter>();
	USkeletalMeshComponent& Mesh = Character->GetMesh();
	TestTrue("Default instance inside the mesh", Mesh.GetAnimInstance().GetOuter() == &Mesh);
	TestNull("Not a character instance", Mesh.GetAnimInstance<UCharacterAnimInstance>());
	UCharacterAnimInstance& CharacterAnim = Mesh.SetAnimInstance<UCharacterAnimInstance>();
	TestTrue("Character instance in use", Mesh.GetAnimInstance<UCharacterAnimInstance>() == &CharacterAnim);
	TestTrue("Owning mesh", CharacterAnim.GetOwningMeshComponent() == &Mesh);
	TestTrue("Still a UAnimInstance", Cast<UAnimInstance>(&CharacterAnim) != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameFrameworkEngineCollectsGarbageOnATimerTest,
	"System.Engine.GameFramework.EngineCollectsGarbageOnATimer",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameFrameworkEngineCollectsGarbageOnATimerTest::RunTest(const FString& Parameters)
{
	// The engine collects once gc.TimeBetweenPurgingPendingKillObjects (61.1 s by default) has passed, not before; the
	// engine's own objects survive, an unreferenced one does not.
	UGameEngine Engine;
	TWeakObjectPtr<UObject> Garbage = NewObject<AActor>();
	TestFalse("Not yet", Engine.ConditionalCollectGarbage(30.0f));
	TestTrue("Garbage still there", Garbage.IsValid());
	TestTrue("Interval reached", Engine.ConditionalCollectGarbage(31.2f));
	TestFalse("Garbage collected", Garbage.IsValid());
	TestNotNull("Engine world kept", Engine.GetWorld());
	TestEqual("Engine camera kept", Engine.GetCamera().GetDistance(), 500.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
