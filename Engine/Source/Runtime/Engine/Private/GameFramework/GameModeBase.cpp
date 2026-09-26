#include "GameFramework/GameModeBase.h"

#include "AI/Navigation/NavigationSystem.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Level.h"
#include "EngineLogs.h"
#include "GameFramework/Character.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/WorldSettings.h"

AGameModeBase::AGameModeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The game mode ticks its game state's clock (Leon: AInfo actors do not tick; UE's game mode ticks).
	bCanEverTick = true;
	GameStateClass = AGameStateBase::StaticClass();
	PlayerControllerClass = APlayerController::StaticClass();
	PlayerStateClass = APlayerState::StaticClass();
	DefaultPawnClass = ADefaultPawn::StaticClass();
	HUDClass = AHUD::StaticClass();
	SpectatorClass = ASpectatorPawn::StaticClass();
	DefaultPlayerName = TEXT("Player");
}

void AGameModeBase::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	SpawnGameState();
}

void AGameModeBase::SpawnGameState()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	if (GameState != nullptr)
	{
		GameState->Destroy();
		GameState = nullptr;
	}
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	GameState = World->SpawnActor<AGameStateBase>(
		GameStateClass != nullptr ? GameStateClass.Get() : AGameStateBase::StaticClass(), SpawnInfo);
	World->SetGameState(GameState);
	InitGameState();
}

void AGameModeBase::InitGame(const FString& /*MapName*/, const FString& Options, FString& ErrorMessage)
{
	ErrorMessage.Empty();
	OptionsString = Options;
}

void AGameModeBase::StartPlay()
{
	GetGameState().HandleBeginPlay();
}

void AGameModeBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GameState != nullptr && !GameState->IsPendingKillPending())
	{
		GameState->Tick(DeltaSeconds);
	}
}

APlayerController* AGameModeBase::Login(
	UPlayer* /*NewPlayer*/, const FString& Portal, const FString& Options, FString& ErrorMessage)
{
	ErrorMessage.Empty();
	APlayerController* const NewPlayerController = SpawnPlayerController(Options);
	if (NewPlayerController == nullptr)
	{
		UE_LOG(LogGameMode, Log, TEXT("Login: Couldn't spawn player controller of class %s"),
			PlayerControllerClass != nullptr ? *PlayerControllerClass->GetName() : TEXT("NULL"));
		ErrorMessage = TEXT("Failed to spawn player controller");
		return nullptr;
	}
	// The player joins with the URL's options.
	ErrorMessage = InitNewPlayer(NewPlayerController, Options, Portal);
	if (!ErrorMessage.IsEmpty())
	{
		NewPlayerController->Destroy();
		return nullptr;
	}
	return NewPlayerController;
}

APlayerController* AGameModeBase::SpawnPlayerController(const FString& /*Options*/)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	// Player controllers are never saved into a map (UE).
	SpawnInfo.ObjectFlags |= RF_Transient;
	UClass* ControllerClass =
		PlayerControllerClass != nullptr ? PlayerControllerClass.Get() : APlayerController::StaticClass();
	return GetWorld()->SpawnActor<APlayerController>(ControllerClass, SpawnInfo);
}

FString AGameModeBase::InitNewPlayer(
	APlayerController* NewPlayerController, const FString& Options, const FString& Portal)
{
	check(NewPlayerController != nullptr);
	APlayerState* PlayerState = NewPlayerController->GetPlayerState<APlayerState>();
	if (PlayerState == nullptr)
	{
		return TEXT("PlayerState is null");
	}

	FString ErrorMessage;
	if (!UpdatePlayerStartSpot(NewPlayerController, Portal, ErrorMessage))
	{
		UE_LOG(LogGameMode, Warning, TEXT("InitNewPlayer: %s"), *ErrorMessage);
	}

	// The player's name: ?Name= in the options, else the default name and the player id (UE).
	FString InName;
	const TCHAR* Found = FCString::Stristr(*Options, TEXT("?Name="));
	if (Found != nullptr)
	{
		const TCHAR* Value = Found + 6;
		while (*Value != '\0' && *Value != '?' && InName.Len() < 20)
		{
			InName += *Value++;
		}
	}
	if (InName.IsEmpty())
	{
		InName = FString::Printf(TEXT("%s%d"), *DefaultPlayerName, PlayerState->GetPlayerId());
	}
	PlayerState->SetPlayerName(InName);
	return FString();
}

bool AGameModeBase::UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage)
{
	OutErrorMessage.Empty();
	AActor* const StartSpot = FindPlayerStart(Player, Portal);
	if (StartSpot != nullptr)
	{
		const FRotator StartRotation(0.0f, StartSpot->GetActorRotation().Yaw, 0.0f);
		Player->SetInitialLocationAndRotation(StartSpot->GetActorLocation(), StartRotation);
		Player->StartSpot = StartSpot;
		return true;
	}
	OutErrorMessage = TEXT("Could not find a starting spot");
	return false;
}

void AGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	if (NewPlayer == nullptr)
	{
		return;
	}
	GenericPlayerInitialization(NewPlayer);
	GetGameState().AddPlayerState(NewPlayer->GetPlayerState<APlayerState>());
	// Now that the player is set up, it may spawn its pawn (UE).
	HandleStartingNewPlayer(NewPlayer);
}

void AGameModeBase::GenericPlayerInitialization(AController* C)
{
	if (APlayerController* PlayerController = Cast<APlayerController>(C))
	{
		InitializeHUDForPlayer(PlayerController);
	}
}

void AGameModeBase::InitializeHUDForPlayer(APlayerController* NewPlayer)
{
	NewPlayer->ClientSetHUD(HUDClass);
}

void AGameModeBase::Logout(AController* Exiting)
{
	if (Exiting != nullptr)
	{
		GetGameState().RemovePlayerState(Exiting->GetPlayerState<APlayerState>());
	}
}

void AGameModeBase::HandleStartingNewPlayer(APlayerController* NewPlayer)
{
	if (PlayerCanRestart(NewPlayer))
	{
		RestartPlayer(NewPlayer);
	}
}

bool AGameModeBase::PlayerCanRestart(APlayerController* Player)
{
	return Player != nullptr && !Player->IsPendingKillPending();
}

void AGameModeBase::RestartPlayer(AController* NewPlayer)
{
	if (NewPlayer == nullptr || NewPlayer->IsPendingKillPending())
	{
		return;
	}
	AActor* StartSpot = FindPlayerStart(NewPlayer);
	if (StartSpot == nullptr && NewPlayer->StartSpot.IsValid())
	{
		// The spot the player had before (UE).
		StartSpot = NewPlayer->StartSpot.Get();
		UE_LOG(LogGameMode, Warning, TEXT("RestartPlayer: Player start not found, using last start spot"));
	}
	RestartPlayerAtPlayerStart(NewPlayer, StartSpot);
}

void AGameModeBase::RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot)
{
	if (NewPlayer == nullptr || NewPlayer->IsPendingKillPending())
	{
		return;
	}
	if (StartSpot == nullptr)
	{
		UE_LOG(LogGameMode, Warning, TEXT("RestartPlayerAtPlayerStart: Player start not found"));
		return;
	}
	FRotator SpawnRotation = StartSpot->GetActorRotation();
	if (APawn* ExistingPawn = NewPlayer->GetPawn())
	{
		// An existing pawn keeps its rotation (UE).
		SpawnRotation = ExistingPawn->GetActorRotation();
	}
	else if (GetDefaultPawnClassForController(NewPlayer) != nullptr)
	{
		APawn* NewPawn = SpawnDefaultPawnFor(NewPlayer, StartSpot);
		if (IsValid(NewPawn))
		{
			// UE sets the pawn here and possesses it in FinishRestartPlayer; Leon's controller takes it by possession.
			NewPlayer->Possess(NewPawn);
		}
	}
	if (!IsValid(NewPlayer->GetPawn()))
	{
		FailedToRestartPlayer(NewPlayer);
	}
	else
	{
		FinishRestartPlayer(NewPlayer, SpawnRotation);
	}
}

void AGameModeBase::RestartPlayerAtTransform(AController* NewPlayer, const FTransform& SpawnTransform)
{
	if (NewPlayer == nullptr || NewPlayer->IsPendingKillPending())
	{
		return;
	}
	FRotator SpawnRotation = SpawnTransform.Rotator();
	if (APawn* ExistingPawn = NewPlayer->GetPawn())
	{
		SpawnRotation = ExistingPawn->GetActorRotation();
	}
	else if (GetDefaultPawnClassForController(NewPlayer) != nullptr)
	{
		APawn* NewPawn = SpawnDefaultPawnAtTransform(NewPlayer, SpawnTransform);
		if (IsValid(NewPawn))
		{
			NewPlayer->Possess(NewPawn);
		}
	}
	if (!IsValid(NewPlayer->GetPawn()))
	{
		FailedToRestartPlayer(NewPlayer);
	}
	else
	{
		FinishRestartPlayer(NewPlayer, SpawnRotation);
	}
}

void AGameModeBase::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	NewPlayer->Possess(NewPlayer->GetPawn());
	if (!IsValid(NewPlayer->GetPawn()))
	{
		FailedToRestartPlayer(NewPlayer);
		return;
	}
	// The control rotation starts at the start's rotation, without roll (UE).
	NewPlayer->ClientSetRotation(NewPlayer->GetPawn()->GetActorRotation());
	FRotator NewControllerRot = StartRotation;
	NewControllerRot.Roll = 0.0f;
	NewPlayer->SetControlRotation(NewControllerRot);
}

void AGameModeBase::FailedToRestartPlayer(AController* NewPlayer)
{
	UE_LOG(LogGameMode, Warning, TEXT("FailedToRestartPlayer: %s"),
		NewPlayer != nullptr ? *NewPlayer->GetName() : TEXT("NULL"));
}

UClass* AGameModeBase::GetDefaultPawnClassForController(AController* /*InController*/)
{
	return DefaultPawnClass.Get();
}

APawn* AGameModeBase::SpawnDefaultPawnFor(AController* NewPlayer, AActor* StartSpot)
{
	// No pitch or roll on a spawned pawn (UE).
	const FRotator StartRotation(0.0f, StartSpot->GetActorRotation().Yaw, 0.0f);
	const FVector StartLocation = StartSpot->GetActorLocation();
	return SpawnDefaultPawnAtTransform(NewPlayer, FTransform(StartRotation, StartLocation));
}

APawn* AGameModeBase::SpawnDefaultPawnAtTransform(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	// Default player pawns are never saved into a map (UE).
	SpawnInfo.ObjectFlags |= RF_Transient;
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	APawn* ResultPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
	if (ResultPawn == nullptr)
	{
		UE_LOG(LogGameMode, Warning, TEXT("SpawnDefaultPawnAtTransform: Couldn't spawn Pawn of type %s"),
			PawnClass != nullptr ? *PawnClass->GetName() : TEXT("NULL"));
	}
	return ResultPawn;
}

namespace
{

	/** The level's live APlayerStart actors, in spawn order. */
	void GetPlayerStarts(const ULevel& Level, TArray<APlayerStart*>& OutStarts)
	{
		for (AActor* Actor : Level.Actors)
		{
			APlayerStart* Start = Cast<APlayerStart>(Actor);
			if (Start != nullptr && !Start->IsPendingKillPending())
			{
				OutStarts.Add(Start);
			}
		}
	}

} // namespace

AActor* AGameModeBase::FindPlayerStart(AController* Player, const FString& IncomingName)
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return nullptr;
	}
	TArray<APlayerStart*> Starts;
	GetPlayerStarts(*World->PersistentLevel, Starts);

	// A named start (the URL portal) is used as it is (UE).
	if (!IncomingName.IsEmpty())
	{
		const FName IncomingPlayerStartTag(*IncomingName);
		for (APlayerStart* Start : Starts)
		{
			if (Start->PlayerStartTag == IncomingPlayerStartTag)
			{
				return Start;
			}
		}
	}

	// The start spot chosen at login, at the start of play (UE).
	if (ShouldSpawnAtStartSpot(Player))
	{
		return Player->StartSpot.Get();
	}

	AActor* BestStart = ChoosePlayerStart(Player);
	if (BestStart == nullptr)
	{
		// UE spawns at the world settings (the origin) without a start.
		UE_LOG(LogGameMode, Log, TEXT("FindPlayerStart: PATHS NOT DEFINED or NO PLAYERSTART with positive rating"));
		BestStart = World->GetWorldSettings();
	}
	return BestStart;
}

AActor* AGameModeBase::ChoosePlayerStart(AController* /*Player*/)
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return nullptr;
	}
	TArray<APlayerStart*> Starts;
	GetPlayerStarts(*World->PersistentLevel, Starts);
	return Starts.Num() > 0 ? Starts[0] : nullptr;
}

bool AGameModeBase::ShouldSpawnAtStartSpot(AController* Player)
{
	return Player != nullptr && Player->StartSpot.IsValid();
}
