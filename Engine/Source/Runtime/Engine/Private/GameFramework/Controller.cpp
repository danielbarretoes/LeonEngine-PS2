#include "GameFramework/Controller.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"

AController::AController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHidden = true;
}

void AController::Possess(APawn* InPawn)
{
	if (Pawn == InPawn)
	{
		return;
	}
	if (InPawn != nullptr)
	{
		if (AController* Previous = InPawn->GetController(); Previous != nullptr && Previous != this)
		{
			Previous->UnPossess();
		}
	}
	UnPossess();
	Pawn = InPawn;
	if (Pawn != nullptr)
	{
		Pawn->BindController(this);
		OnPossess(Pawn);
	}
}

void AController::UnPossess()
{
	if (Pawn == nullptr)
	{
		return;
	}
	Pawn->BindController(nullptr);
	Pawn = nullptr;
	OnUnPossess();
}

void AController::OnPossess(APawn* /*InPawn*/)
{
}

void AController::OnUnPossess()
{
}

void AController::SetInitialLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
{
	SetActorLocationAndRotation(NewLocation, NewRotation);
	SetControlRotation(NewRotation);
}

void AController::ClientSetRotation(const FRotator& NewRotation, bool /*bResetCamera*/)
{
	SetControlRotation(NewRotation);
}

ACharacter* AController::GetCharacter() const
{
	return Cast<ACharacter>(Pawn);
}

void AController::InitPlayerState()
{
	UWorld* World = GetWorld();
	if (World == nullptr || PlayerState != nullptr)
	{
		return;
	}
	TSubclassOf<APlayerState> PlayerStateClass = APlayerState::StaticClass();
	if (const AGameModeBase* GameMode = World->GetAuthGameMode())
	{
		if (GameMode->PlayerStateClass != nullptr)
		{
			PlayerStateClass = GameMode->PlayerStateClass;
		}
	}
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	PlayerState = World->SpawnActor<APlayerState>(PlayerStateClass, SpawnInfo);
}

void AController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (bWantsPlayerState && !IsPendingKill())
	{
		InitPlayerState();
	}
}

void AController::Destroyed()
{
	UnPossess();
	if (PlayerState != nullptr)
	{
		PlayerState->Destroy();
		PlayerState = nullptr;
	}
	Super::Destroyed();
}

void AController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnPossess();
	Super::EndPlay(EndPlayReason);
}
