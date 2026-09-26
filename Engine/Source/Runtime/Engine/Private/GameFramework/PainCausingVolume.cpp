#include "GameFramework/PainCausingVolume.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

APainCausingVolume::APainCausingVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// AVolume does not tick; the pain ticks do (UE's PainTimer).
	bCanEverTick = true;
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

void APainCausingVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	if (!bPainCausing || PainInterval <= 0.0f || World == nullptr || World->PersistentLevel == nullptr)
	{
		return;
	}
	TimeUntilPain -= DeltaSeconds;
	if (TimeUntilPain > 0.0f)
	{
		return;
	}
	TimeUntilPain += PainInterval;
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
