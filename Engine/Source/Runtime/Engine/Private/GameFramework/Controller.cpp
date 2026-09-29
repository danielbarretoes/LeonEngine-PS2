#include "GameFramework/Controller.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "Stats/Stats.h"

DECLARE_CYCLE_STAT(TEXT("Possess"), STAT_Possess, STATGROUP_Engine);

const FName NAME_Inactive(TEXT("Inactive"));
const FName NAME_Playing(TEXT("Playing"));
const FName NAME_Spectating(TEXT("Spectating"));

AController::AController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHidden = true;
}

void AController::Possess(APawn* InPawn)
{
	SCOPE_CYCLE_COUNTER(STAT_Possess);
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
		AddPawnTickDependency(Pawn);
		Pawn->PossessedBy(this);
		OnPossess(Pawn);
	}
}

void AController::AddPawnTickDependency(APawn* NewPawn)
{
	// UE: the pawn (and, through it, its components: FActorComponentTickFunction) ticks after its controller, so its
	// movement uses the input the controller processed in the same step.
	if (NewPawn != nullptr)
	{
		NewPawn->PrimaryActorTick.AddPrerequisite(this, PrimaryActorTick);
	}
}

void AController::RemovePawnTickDependency(APawn* InOldPawn)
{
	if (InOldPawn != nullptr)
	{
		InOldPawn->PrimaryActorTick.RemovePrerequisite(this, PrimaryActorTick);
	}
}

void AController::UnPossess()
{
	if (Pawn == nullptr)
	{
		return;
	}
	APawn* OldPawn = Pawn;
	RemovePawnTickDependency(OldPawn);
	OldPawn->BindController(nullptr);
	Pawn = nullptr;
	OldPawn->UnPossessed();
	OnUnPossess();
}

void AController::OnPossess(APawn* /*InPawn*/)
{
}

void AController::ChangeState(FName NewState)
{
	StateName = NewState;
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
