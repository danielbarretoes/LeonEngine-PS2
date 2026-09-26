#include "Perception/PawnSensingComponent.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Physics/PhysScene.h"

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

} // namespace

UPawnSensingComponent::UPawnSensingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetPeripheralVisionAngle(PeripheralVisionAngle);
	SetComponentTickEnabled(true);
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
		FHitResult Hit;
		if (!World->GetPhysicsScene().LineTraceSingleByChannel(Hit, From, Target, ECC_Visibility, Query))
		{
			return true;
		}
	}
	return false;
}

bool UPawnSensingComponent::CouldSeePawn(const APawn* Other, bool bMaySkipChecks) const
{
	const APawn* Self = GetSensingPawn(*this);
	if (Other == nullptr || Other == Self || Other->IsPendingKillPending())
	{
		return false;
	}
	if (bOnlySensePlayers && Cast<APlayerController>(Other->GetController()) == nullptr)
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
	const UWorld* World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr || !bEnableSensingUpdates || !bSeePawns)
	{
		return;
	}
	// The pawns first: a listener may destroy actors.
	TArray<APawn*> Seen;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (Pawn != nullptr && CouldSeePawn(Pawn))
		{
			Seen.Add(Pawn);
		}
	}
	for (APawn* Pawn : Seen)
	{
		OnSeePawn.Broadcast(Pawn);
	}
}

void UPawnSensingComponent::HandleNoise(APawn* Instigator, const FVector& Location, float Loudness)
{
	if (!bEnableSensingUpdates || !bHearNoises || Instigator == GetSensingPawn(*this) || Loudness <= 0.0f)
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
	if (World.PersistentLevel == nullptr)
	{
		return;
	}
	TArray<UPawnSensingComponent*> Listeners;
	TArray<UPawnSensingComponent*> ActorListeners;
	for (AActor* Actor : World.PersistentLevel->Actors)
	{
		if (Actor != nullptr && !Actor->IsPendingKillPending())
		{
			Actor->GetComponents(ActorListeners);
			Listeners.Append(ActorListeners);
		}
	}
	for (UPawnSensingComponent* Listener : Listeners)
	{
		Listener->HandleNoise(Instigator, Location, Loudness);
	}
}

void UPawnSensingComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	TimeUntilNextUpdate -= DeltaTime;
	if (TimeUntilNextUpdate <= 0.0f)
	{
		TimeUntilNextUpdate += FMath::Max(SensingInterval, 0.01f);
		if (TimeUntilNextUpdate < 0.0f)
		{
			TimeUntilNextUpdate = SensingInterval;
		}
		UpdateAISensing();
	}
}
