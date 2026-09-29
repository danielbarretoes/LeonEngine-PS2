#include "GameFramework/PainCausingVolume.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

APainCausingVolume::APainCausingVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bPainCausing = true;
}

void APainCausingVolume::CausePainTo(AActor* Other)
{
	const float Amount = DamagePerSec * PainInterval;
	if (Other == nullptr || Amount <= 0.0f)
	{
		return;
	}
	// UE: TakeDamage with a plain damage event of the volume's damage type, the volume the causer.
	(void)UGameplayStatics::ApplyDamage(Other, Amount, DamageInstigator, this, DamageType);
}

void APainCausingVolume::BeginPlay()
{
	Super::BeginPlay();
	if (PainInterval > 0.0f)
	{
		// The first pain at the next step, then one every PainInterval.
		GetWorldTimerManager().SetTimer(
			TimerHandle_PainTimer, this, &APainCausingVolume::PainTimer, PainInterval, true, 0.0f);
	}
}

void APainCausingVolume::PainTimer()
{
	const UWorld* World = GetWorld();
	if (!bPainCausing || World == nullptr || World->PersistentLevel == nullptr)
	{
		return;
	}
	// A copy: the damage may destroy pawns (the level's array changes).
	const TArray<AActor*> Actors = World->PersistentLevel->Actors;
	for (AActor* Actor : Actors)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (Pawn != nullptr && !Pawn->IsPendingKillPending() && EncompassesPoint(Pawn->GetActorLocation()))
		{
			CausePainTo(Pawn);
		}
	}
}
