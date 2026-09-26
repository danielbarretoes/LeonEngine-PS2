#include "GameFramework/PlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Engine/Player.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/SpectatorPawn.h"

APlayerController::APlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = true;
	PlayerCameraManagerClass = APlayerCameraManager::StaticClass();
}

void APlayerController::Possess(ACharacter* Character)
{
	AController::Possess(Character);
}

void APlayerController::SetPlayer(UPlayer* InPlayer)
{
	check(InPlayer != nullptr);
	Player = InPlayer;
	InPlayer->PlayerController = this;
	// A local player's controller gets its input and its camera (UE spawns the camera in PostInitializeComponents for
	// every controller; Leon only for the local ones).
	InitInputSystem();
	if (PlayerCameraManager == nullptr)
	{
		SpawnPlayerCameraManager();
	}
}

bool APlayerController::IsLocalController() const
{
	return Player != nullptr;
}

void APlayerController::InitInputSystem()
{
	if (PlayerInput == nullptr)
	{
		PlayerInput = NewObject<UPlayerInput>(this, UInputSettings::GetDefaultPlayerInputClass());
	}
	SetupInputComponent();
}

void APlayerController::SetupInputComponent()
{
	if (InputComponent == nullptr)
	{
		static const FName InputComponentName(TEXT("PC_InputComponent0"));
		InputComponent =
			NewObject<UInputComponent>(this, UInputSettings::GetDefaultInputComponentClass(), InputComponentName);
		InputComponent->RegisterComponent();
	}
}

void APlayerController::SpawnPlayerCameraManager()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = GetInstigator();
	// Cameras are never saved into a map (UE).
	SpawnInfo.ObjectFlags |= RF_Transient;
	UClass* CameraClass =
		PlayerCameraManagerClass != nullptr ? PlayerCameraManagerClass.Get() : APlayerCameraManager::StaticClass();
	PlayerCameraManager = World->SpawnActor<APlayerCameraManager>(CameraClass, SpawnInfo);
	if (PlayerCameraManager != nullptr)
	{
		PlayerCameraManager->InitializeFor(this);
	}
}

void APlayerController::ClientSetHUD(TSubclassOf<AHUD> NewHUDClass)
{
	if (MyHUD != nullptr)
	{
		MyHUD->Destroy();
		MyHUD = nullptr;
	}
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = GetInstigator();
	// HUDs are never saved into a map (UE).
	SpawnInfo.ObjectFlags |= RF_Transient;
	MyHUD = World->SpawnActor<AHUD>(NewHUDClass != nullptr ? NewHUDClass.Get() : AHUD::StaticClass(), SpawnInfo);
}

bool APlayerController::InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	return PlayerInput != nullptr && PlayerInput->InputKey(Key, EventType, AmountDepressed, bGamepad);
}

bool APlayerController::InputAxis(FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	return PlayerInput != nullptr && PlayerInput->InputAxis(Key, Delta, DeltaTime, NumSamples, bGamepad);
}

void APlayerController::PushInputComponent(UInputComponent* Input)
{
	if (Input == nullptr)
	{
		return;
	}
	CurrentInputStack.Remove(Input);
	// Higher priorities sit higher on the stack (UE).
	int32 InsertIndex = CurrentInputStack.Num();
	while (InsertIndex > 0 && CurrentInputStack[InsertIndex - 1] != nullptr &&
		CurrentInputStack[InsertIndex - 1]->Priority > Input->Priority)
	{
		--InsertIndex;
	}
	CurrentInputStack.Insert(Input, InsertIndex);
}

bool APlayerController::PopInputComponent(UInputComponent* Input)
{
	if (Input == nullptr || CurrentInputStack.Remove(Input) == 0)
	{
		return false;
	}
	Input->ClearBindingValues();
	return true;
}

void APlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Player != nullptr && PlayerInput != nullptr)
	{
		PlayerTick(DeltaSeconds);
	}
}

void APlayerController::PlayerTick(float DeltaTime)
{
	TickPlayerInput(DeltaTime, false);
	if (Player != nullptr && Player->PlayerController == this)
	{
		UpdateRotation(DeltaTime);
	}
}

void APlayerController::TickPlayerInput(const float DeltaSeconds, const bool bGamePaused)
{
	ProcessPlayerInput(DeltaSeconds, bGamePaused);
}

void APlayerController::ProcessPlayerInput(const float DeltaTime, const bool bGamePaused)
{
	TArray<UInputComponent*> InputStack;
	BuildInputStack(InputStack);
	PlayerInput->ProcessInputStack(InputStack, DeltaTime, bGamePaused);
}

void APlayerController::BuildInputStack(TArray<UInputComponent*>& InputStack)
{
	// The pawn's component at the bottom (it gets the last say), then the controller's, then the pushed ones (UE).
	if (APawn* ControlledPawn = GetPawn())
	{
		if (ControlledPawn->InputComponent != nullptr)
		{
			InputStack.Add(ControlledPawn->InputComponent);
		}
		for (UActorComponent* Component : ControlledPawn->GetComponents())
		{
			UInputComponent* PawnInputComponent = Cast<UInputComponent>(Component);
			if (PawnInputComponent != nullptr && PawnInputComponent != ControlledPawn->InputComponent)
			{
				InputStack.Add(PawnInputComponent);
			}
		}
	}
	if (InputComponent != nullptr)
	{
		InputStack.Add(InputComponent);
	}
	for (int32 Index = 0; Index < CurrentInputStack.Num(); ++Index)
	{
		if (UInputComponent* Pushed = CurrentInputStack[Index])
		{
			InputStack.Add(Pushed);
		}
		else
		{
			CurrentInputStack.RemoveAt(Index--);
		}
	}
}

void APlayerController::UpdateRotation(float DeltaTime)
{
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->FaceRotation(GetControlRotation(), DeltaTime);
	}
}

void APlayerController::UpdateCameraManager(float DeltaSeconds)
{
	if (PlayerCameraManager != nullptr && !PlayerCameraManager->IsPendingKillPending())
	{
		PlayerCameraManager->UpdateCamera(DeltaSeconds);
	}
}

void APlayerController::GetPlayerViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	if (PlayerCameraManager != nullptr)
	{
		OutLocation = PlayerCameraManager->GetCameraLocation();
		OutRotation = PlayerCameraManager->GetCameraRotation();
		return;
	}
	OutLocation = GetActorLocation();
	OutRotation = GetControlRotation();
}

void APlayerController::ClientRestart(APawn* NewPawn)
{
	if (NewPawn != nullptr && NewPawn == GetPawn())
	{
		NewPawn->PawnClientRestart();
	}
}

void APlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// A pawn of its own ends the spectating (UE: OnPossess → ChangeState(NAME_Playing)).
	if (InPawn != nullptr && InPawn != SpectatorPawn && !IsInState(NAME_Playing))
	{
		ChangeState(NAME_Playing);
	}
	if (IsLocalController())
	{
		ClientRestart(InPawn);
	}
}

void APlayerController::ChangeState(FName NewState)
{
	if (NewState == StateName)
	{
		return;
	}
	if (StateName == NAME_Spectating)
	{
		EndSpectatingState();
	}
	Super::ChangeState(NewState);
	if (StateName == NAME_Spectating)
	{
		BeginSpectatingState();
	}
}

void APlayerController::BeginSpectatingState()
{
	// The view point before the pawn goes (UE: GetSpawnLocation, the last view): the camera's, else the pawn's eyes.
	FVector ViewLocation;
	FRotator ViewRotation;
	if (PlayerCameraManager == nullptr && GetPawn() != nullptr)
	{
		GetPawn()->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	if (GetPawn() != nullptr)
	{
		UnPossess();
	}
	if (PlayerCameraManager != nullptr)
	{
		PlayerCameraManager->SetViewTarget(nullptr);
	}
	DestroySpectatorPawn();
	SetActorLocation(ViewLocation);
	SetSpectatorPawn(SpawnSpectatorPawn());
}

void APlayerController::EndSpectatingState()
{
	DestroySpectatorPawn();
}

ASpectatorPawn* APlayerController::SpawnSpectatorPawn()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}
	const AGameModeBase* GameMode = World->GetAuthGameMode();
	UClass* SpectatorClass = GameMode != nullptr && GameMode->SpectatorClass != nullptr ? GameMode->SpectatorClass.Get()
																						: ASpectatorPawn::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.ObjectFlags |= RF_Transient;
	const FRotator Rotation = GetControlRotation();
	return World->SpawnActor<ASpectatorPawn>(SpectatorClass, GetActorLocation(), Rotation, SpawnParams);
}

void APlayerController::DestroySpectatorPawn()
{
	if (SpectatorPawn == nullptr)
	{
		return;
	}
	ASpectatorPawn* OldSpectator = SpectatorPawn;
	SpectatorPawn = nullptr;
	if (GetPawn() == OldSpectator)
	{
		UnPossess();
	}
	OldSpectator->Destroy();
}

void APlayerController::SetSpectatorPawn(ASpectatorPawn* NewSpectatorPawn)
{
	if (!IsInState(NAME_Spectating))
	{
		return;
	}
	SpectatorPawn = NewSpectatorPawn;
	if (NewSpectatorPawn != nullptr)
	{
		// Leon: the player flies the spectator as its pawn (UE views it and routes the input to it).
		Possess(NewSpectatorPawn);
	}
}

void APlayerController::FOV(float NewFOV)
{
	if (PlayerCameraManager == nullptr)
	{
		return;
	}
	if (NewFOV > 0.0f)
	{
		PlayerCameraManager->SetFOV(NewFOV);
	}
	else
	{
		PlayerCameraManager->UnlockFOV();
	}
}

void APlayerController::Destroyed()
{
	// UE: the spectator goes with its controller.
	DestroySpectatorPawn();
	if (MyHUD != nullptr)
	{
		MyHUD->Destroy();
		MyHUD = nullptr;
	}
	if (PlayerCameraManager != nullptr)
	{
		PlayerCameraManager->Destroy();
		PlayerCameraManager = nullptr;
	}
	Super::Destroyed();
}

void APlayerController::AddYawInput(float Val)
{
	FRotator Rotation = GetControlRotation();
	Rotation.Yaw += Val;
	SetControlRotation(Rotation);
}

void APlayerController::AddPitchInput(float Val)
{
	FRotator Rotation = GetControlRotation();
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch + Val, ViewPitchMin, ViewPitchMax);
	SetControlRotation(Rotation);
}
