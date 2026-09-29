#include "ShooterPawnSensingComponent.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"

namespace
{

	/** The bot's pawn: the owner controller's (or the owner itself), as a shooter. */
	const AShooterCharacter* GetSensingShooter(const UActorComponent& Component)
	{
		const AActor* Owner = Component.GetOwner();
		if (const AController* Controller = Cast<AController>(Owner))
		{
			return Cast<AShooterCharacter>(Controller->GetPawn());
		}
		return Cast<AShooterCharacter>(Owner);
	}

} // namespace

UShooterPawnSensingComponent::UShooterPawnSensingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UShooterPawnSensingComponent::ShouldCheckVisibilityOf(const APawn* Pawn) const
{
	if (!Super::ShouldCheckVisibilityOf(Pawn))
	{
		return false;
	}
	// A frozen bot sees nobody (its reaction starts with the round); a dead one has no eyes.
	const AShooterCharacter* Self = GetSensingShooter(*this);
	const AShooterCharacter* Other = Cast<AShooterCharacter>(Pawn);
	// Blinded by a flashbang: nobody (CS's bots).
	return Self != nullptr && Other != nullptr && Self->IsAlive() && !Self->IsFrozen() && !Self->IsBlind() &&
		Other->IsAlive() && Other->GetTeam() != EShooterTeam::None && Other->GetTeam() != Self->GetTeam();
}

bool UShooterPawnSensingComponent::HasLineOfSightTo(const AActor* Other) const
{
	if (!Super::HasLineOfSightTo(Other))
	{
		return false;
	}
	// A smoke cloud hides what is behind it: the line to the other's eyes goes through its sphere.
	const UWorld* World = GetWorld();
	const AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	if (GameMode == nullptr || GameMode->GetSmokeClouds().Num() == 0)
	{
		return true;
	}
	const APawn* OtherPawn = Cast<APawn>(Other);
	const FVector Target = OtherPawn != nullptr ? OtherPawn->GetPawnViewLocation() : Other->GetActorLocation();
	return !GameMode->IsSightBlockedBySmoke(GetSensorLocation(), Target);
}

void UShooterPawnSensingComponent::OnTimer()
{
	// A bot that sees nobody now (dead, frozen) looks for free: it traces nothing.
	const AShooterCharacter* Self = GetSensingShooter(*this);
	const UWorld* World = GetWorld();
	AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	if (GameMode != nullptr)
	{
		if (Self == nullptr || !Self->IsAlive() || Self->IsFrozen())
		{
			GameMode->CancelSensingUpdate(this);
		}
		else if (!GameMode->ClaimSensingUpdate(this))
		{
			RetryOnNextTick();
			return;
		}
	}
	Super::OnTimer();
}

void UShooterPawnSensingComponent::OnUnregister()
{
	const UWorld* World = GetWorld();
	if (AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr)
	{
		GameMode->CancelSensingUpdate(this);
	}
	Super::OnUnregister();
}

bool UShooterPawnSensingComponent::ShouldCheckAudibilityOf(const APawn* NoiseInstigator) const
{
	if (!Super::ShouldCheckAudibilityOf(NoiseInstigator))
	{
		return false;
	}
	const AShooterCharacter* Self = GetSensingShooter(*this);
	const AShooterCharacter* Heard = Cast<AShooterCharacter>(NoiseInstigator);
	return Self != nullptr && Heard != nullptr && Heard->GetTeam() != Self->GetTeam();
}
