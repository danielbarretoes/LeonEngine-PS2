#include "Perception/PawnSensingComponent.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/LowLevelMemTracker.h"
#include "Physics/PhysScene.h"
#include "Stats/Stats.h"
#include "TimerManager.h"

DECLARE_CYCLE_STAT(TEXT("Pawn Sensing"), STAT_PawnSensing, STATGROUP_AI);

namespace
{

	/** The pawn the component senses for: its owner, or its owner controller's pawn. */
	const APawn* GetSensingPawn(const UActorComponent& Component)
	{
		const AActor* Owner = Component.GetOwner();
		if (const APawn* Pawn = Cast<APawn>(Owner))
		{
			return Pawn;
		}
		const AController* Controller = Cast<AController>(Owner);
		return Controller != nullptr ? Controller->GetPawn() : nullptr;
	}

	/**
	 * The registered sensing components of every world, in the order they registered: the listeners BroadcastNoise
	 * visits (a handful: one a bot). A component leaves it when it unregisters (its actor's destruction, its own
	 * BeginDestroy).
	 */
	TArray<UPawnSensingComponent*>& GetListeners()
	{
		static TArray<UPawnSensingComponent*> Listeners;
		return Listeners;
	}

	/** Room for a match's sensors and the pawns one sees without an allocation. */
	constexpr int32 InlineSensingCount = 16;

} // namespace

UPawnSensingComponent::UPawnSensingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetPeripheralVisionAngle(PeripheralVisionAngle);
	// The tick only runs a look held back to the next step (RetryOnNextTick).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPawnSensingComponent::BeginPlay()
{
	Super::BeginPlay();
	// A game that spreads its sensors sets their phase after this (SetTimer).
	SetTimer(0.0f);
}

void UPawnSensingComponent::SetPeripheralVisionAngle(float NewPeripheralVisionAngle)
{
	PeripheralVisionAngle = FMath::Clamp(NewPeripheralVisionAngle, 0.0f, 180.0f);
	PeripheralVisionCosine = FMath::Cos(FMath::DegreesToRadians(PeripheralVisionAngle));
}

FVector UPawnSensingComponent::GetSensorLocation() const
{
	if (const APawn* Pawn = GetSensingPawn(*this))
	{
		return Pawn->GetPawnViewLocation();
	}
	return GetOwner() != nullptr ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

FRotator UPawnSensingComponent::GetSensorRotation() const
{
	if (const APawn* Pawn = GetSensingPawn(*this))
	{
		return Pawn->GetViewRotation();
	}
	return GetOwner() != nullptr ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
}

bool UPawnSensingComponent::HasLineOfSightTo(const AActor* Other) const
{
	const UWorld* World = GetWorld();
	if (Other == nullptr || World == nullptr)
	{
		return false;
	}
	FCollisionQueryParams Query(FName(TEXT("PawnSensing")));
	Query.AddIgnoredActor(GetOwner());
	Query.AddIgnoredActor(GetSensingPawn(*this));
	Query.AddIgnoredActor(Other);
	const FVector From = GetSensorLocation();
	const APawn* OtherPawn = Cast<APawn>(Other);
	const FVector Targets[2] = {
		OtherPawn != nullptr ? OtherPawn->GetPawnViewLocation() : Other->GetActorLocation(), Other->GetActorLocation()};
	for (const FVector& Target : Targets)
	{
		++NumSightTraces;
		FHitResult Hit;
		if (!World->GetPhysicsScene().LineTraceSingleByChannel(Hit, From, Target, ECC_Visibility, Query))
		{
			return true;
		}
	}
	return false;
}

bool UPawnSensingComponent::ShouldCheckVisibilityOf(const APawn* Pawn) const
{
	return bSeePawns && Pawn != nullptr &&
		(!bOnlySensePlayers || Cast<APlayerController>(Pawn->GetController()) != nullptr);
}

bool UPawnSensingComponent::ShouldCheckAudibilityOf(const APawn* /*NoiseInstigator*/) const
{
	return bHearNoises;
}

bool UPawnSensingComponent::CouldSeePawn(const APawn* Other, bool bMaySkipChecks) const
{
	const APawn* Self = GetSensingPawn(*this);
	if (Other == nullptr || Other == Self || Other->IsPendingKillPending())
	{
		return false;
	}
	const FVector Sensor = GetSensorLocation();
	const FVector ToOther = Other->GetPawnViewLocation() - Sensor;
	if (ToOther.SizeSquared() > FMath::Square(SightRadius))
	{
		return false;
	}
	const FVector Forward = GetSensorRotation().Vector();
	if (FVector::DotProduct(ToOther.GetSafeNormal(), Forward) < PeripheralVisionCosine)
	{
		return false;
	}
	return bMaySkipChecks || HasLineOfSightTo(Other);
}

void UPawnSensingComponent::UpdateAISensing()
{
	SCOPE_CYCLE_COUNTER(STAT_PawnSensing);
	LLM_SCOPE(ELLMTag::AI);
	const UWorld* World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr || !bEnableSensingUpdates || !bSeePawns)
	{
		return;
	}
	++NumSightUpdates;
	// The pawns first: a listener may destroy actors. The filter comes before the range, the cone and the traces.
	TArray<APawn*, TInlineAllocator<InlineSensingCount>> Seen;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (Pawn != nullptr && ShouldCheckVisibilityOf(Pawn) && CouldSeePawn(Pawn))
		{
			Seen.Add(Pawn);
		}
	}
	// A listener may destroy actors (its own owner included): the ones gone meanwhile are not reported.
	const AActor* Owner = GetOwner();
	for (APawn* Pawn : Seen)
	{
		if (Owner == nullptr || Owner->IsPendingKillPending())
		{
			return;
		}
		if (!Pawn->IsPendingKillPending())
		{
			OnSeePawn.Broadcast(Pawn);
		}
	}
}

void UPawnSensingComponent::HandleNoise(APawn* Instigator, const FVector& Location, float Loudness)
{
	const AActor* Owner = GetOwner();
	if (!bEnableSensingUpdates || Owner == nullptr || Owner->IsPendingKillPending() ||
		Instigator == GetSensingPawn(*this) || Loudness <= 0.0f || !ShouldCheckAudibilityOf(Instigator))
	{
		return;
	}
	const float DistSquared = FVector::DistSquared(GetSensorLocation(), Location);
	if (DistSquared <= FMath::Square(HearingThreshold * Loudness))
	{
		OnHearNoise.Broadcast(Instigator, Location, Loudness);
		return;
	}
	if (DistSquared > FMath::Square(LOSHearingThreshold * Loudness))
	{
		return;
	}
	const UWorld* World = GetWorld();
	FCollisionQueryParams Query(FName(TEXT("PawnHearing")));
	Query.AddIgnoredActor(GetSensingPawn(*this));
	Query.AddIgnoredActor(Instigator);
	FHitResult Hit;
	if (World != nullptr &&
		!World->GetPhysicsScene().LineTraceSingleByChannel(Hit, GetSensorLocation(), Location, ECC_Visibility, Query))
	{
		OnHearNoise.Broadcast(Instigator, Location, Loudness);
	}
}

void UPawnSensingComponent::BroadcastNoise(UWorld& World, APawn* Instigator, const FVector& Location, float Loudness)
{
	// The world's listeners first: a listener may register or unregister others.
	TArray<UPawnSensingComponent*, TInlineAllocator<InlineSensingCount>> Listeners;
	for (UPawnSensingComponent* Listener : GetListeners())
	{
		if (Listener->GetWorld() == &World)
		{
			Listeners.Add(Listener);
		}
	}
	for (UPawnSensingComponent* Listener : Listeners)
	{
		if (Listener->IsRegistered())
		{
			Listener->HandleNoise(Instigator, Location, Loudness);
		}
	}
}

void UPawnSensingComponent::OnRegister()
{
	Super::OnRegister();
	GetListeners().AddUnique(this);
}

void UPawnSensingComponent::OnUnregister()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_OnTimer);
	}
	GetListeners().Remove(this);
	Super::OnUnregister();
}

void UPawnSensingComponent::SetTimer(float TimeInterval)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	// A looping timer keeps the interval's phase; a first delay of 0 is the next step.
	World->GetTimerManager().SetTimer(TimerHandle_OnTimer, this, &UPawnSensingComponent::OnTimer,
		FMath::Max(SensingInterval, 0.01f), true, FMath::Max(0.0f, TimeInterval));
}

void UPawnSensingComponent::RetryOnNextTick()
{
	SetComponentTickEnabled(true);
}

void UPawnSensingComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	SetComponentTickEnabled(false);
	OnTimer();
}

void UPawnSensingComponent::OnTimer()
{
	UpdateAISensing();
}
