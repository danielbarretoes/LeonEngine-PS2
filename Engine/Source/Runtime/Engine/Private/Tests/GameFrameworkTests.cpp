#include "Animation/CharacterAnimInstance.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameState.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpectatorPawn.h"
#include "Misc/AutomationTest.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
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
	GameMode->PostLogin(Player);
	TestEqual("One player", GameMode->GetNumPlayers(), 1);
	TestTrue("In the player array", GameMode->GetGameState().HasPlayerState(PlayerState));
	GameMode->Logout(Player);
	TestEqual("No players", GameMode->GetNumPlayers(), 0);
	GameMode->PostLogin(Player);

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
	// The whole seconds are UE's one-second DefaultTimer; the base clock is the world's time since the start.
	World.Tick(1.5f);
	World.Tick(1.0f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameFrameworkRestartReplacesTheSpectatorTest,
	"System.Engine.GameFramework.RestartReplacesTheSpectator",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameFrameworkRestartReplacesTheSpectatorTest::RunTest(const FString& Parameters)
{
	// A spectating player flies its spectator pawn; a restart gives it a new default pawn at the start and it plays
	// again (UE: a spectator is no pawn to keep). A player that plays keeps its pawn.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AGameModeBase* GameMode = World.SetGameMode(AGameModeBase::StaticClass());
	(void)World.SpawnActor<APlayerStart>(FVector(300.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
	APlayerController* Player = World.SpawnActor<APlayerController>();
	GameMode->RestartPlayer(Player);
	APawn* FirstPawn = Player->GetPawn();
	if (!TestNotNull("A pawn", FirstPawn))
	{
		return false;
	}
	GameMode->RestartPlayer(Player);
	TestTrue("A player that plays keeps its pawn", Player->GetPawn() == FirstPawn);

	Player->ChangeState(NAME_Spectating);
	ASpectatorPawn* Spectator = Player->GetSpectatorPawn();
	TestTrue("It flies the spectator", Spectator != nullptr && Player->GetPawn() == Spectator);
	GameMode->RestartPlayer(Player);
	APawn* NewPawn = Player->GetPawn();
	TestTrue("A new pawn", NewPawn != nullptr && NewPawn != Spectator && NewPawn != FirstPawn);
	TestTrue("A default pawn", NewPawn != nullptr && NewPawn->IsA<ADefaultPawn>());
	TestTrue("Playing again", Player->IsInState(NAME_Playing));
	TestNull("No spectator", Player->GetSpectatorPawn());
	TestTrue("The spectator is gone", Spectator == nullptr || Spectator->IsPendingKillPending());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameFrameworkHUDWidgetsAreObjectsTest,
	"System.Engine.GameFramework.HUDWidgetsAreObjects",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameFrameworkHUDWidgetsAreObjectsTest::RunTest(const FString& Parameters)
{
	// HUD widgets are UObjects inside their HUD, found by class with Cast, with their widget tree (and its widgets)
	// inside them; a removed widget is collected with its tree.
	AHUD* Hud = NewObject<AHUD>();
	Hud->AddToRoot();
	UUserWidget* Widget = Hud->AddWidget<UUserWidget>();
	TestTrue("Widget inside the HUD", Widget->GetOuter() == Hud);
	TestTrue("Owning HUD", Widget->GetOwningHUD() == Hud);
	TestTrue("Found by class", Hud->GetWidgetOfClass<UUserWidget>() == Widget);
	if (!TestTrue("Its tree", Widget->WidgetTree != nullptr && Widget->WidgetTree->GetOuter() == Widget))
	{
		return false;
	}
	TestFalse("Initialized once", Widget->Initialize());
	UTextBlock* Text = Widget->WidgetTree->ConstructWidget<UTextBlock>();
	Widget->WidgetTree->RootWidget = Text;
	TestTrue("A tree widget inside the tree", Text->GetOuter() == Widget->WidgetTree);

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue(
		"Kept by the HUD", Hud->GetWidgetOfClass<UUserWidget>() == Widget && Widget->WidgetTree->RootWidget == Text);
	TWeakObjectPtr<UUserWidget> WeakWidget = Widget;
	TWeakObjectPtr<UTextBlock> WeakText = Text;
	TestTrue("Removed by class", Hud->RemoveWidget<UUserWidget>());
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse("Collected once removed", WeakWidget.IsValid() || WeakText.IsValid());
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
	// The engine starts an incremental collection once gc.TimeBetweenPurgingPendingKillObjects (10 s by default) has
	// passed and ends it over the next steps; the engine's own objects survive, an unreferenced one does not. A forced
	// collection is a full one at the next step.
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	Engine->Init(nullptr);
	TWeakObjectPtr<UObject> Garbage = NewObject<AActor>();
	TestFalse("Not yet", Engine->ConditionalCollectGarbage(5.0f));
	TestTrue("Garbage still there", Garbage.IsValid());
	bool bCollected = Engine->ConditionalCollectGarbage(5.1f);
	for (int32 Step = 0; Step < 1000 && !bCollected; ++Step)
	{
		TestTrue("Alive while the collection runs", Garbage.IsValid());
		bCollected = Engine->ConditionalCollectGarbage(1.0f / 30.0f);
	}
	TestTrue("Interval reached, the collection ended", bCollected);
	TestFalse("Garbage collected", Garbage.IsValid());
	TWeakObjectPtr<UObject> More = NewObject<AActor>();
	Engine->ForceGarbageCollection(true);
	TestTrue("A forced collection ends at once", Engine->ConditionalCollectGarbage(1.0f / 30.0f));
	TestFalse("More garbage collected", More.IsValid());
	TestNotNull("Engine world kept", Engine->GetGameWorld());
	TestNotNull("Engine local player kept", Engine->GameInstance->GetFirstGamePlayer());
	Engine->PreExit();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
