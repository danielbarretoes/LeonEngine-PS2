#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "Templates/SubclassOf.h"
#include "PainCausingVolume.generated.h"

class AController;
class UDamageType;

/**
 * A volume that damages the pawns inside it (UE: APainCausingVolume, an APhysicsVolume there; Leon has no physics
 * volumes). While bPainCausing, every PainInterval seconds each pawn whose location it encompasses takes
 * DamagePerSec * PainInterval of DamageType (CausePainTo), from a looping timer that starts with play (UE's
 * PainTimer; the first pain comes at the first step).
 */
UCLASS()
class ENGINE_API APainCausingVolume : public AVolume
{
	GENERATED_BODY()

public:
	APainCausingVolume(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Whether the volume hurts at all (UE: bPainCausing). */
	UPROPERTY()
	uint8 bPainCausing : 1;

	/** Damage per second (UE: DamagePerSec). */
	UPROPERTY()
	float DamagePerSec = 1.0f;

	/** Seconds between two pain ticks (UE: PainInterval). */
	UPROPERTY()
	float PainInterval = 1.0f;

	/** The kind of damage (UE: DamageType); null is UDamageType. */
	UPROPERTY()
	TSubclassOf<UDamageType> DamageType;

	/** The controller the damage is credited to (UE: DamageInstigator), none by default. */
	UPROPERTY(Transient)
	AController* DamageInstigator = nullptr;

	/** One pain tick to Other: DamagePerSec * PainInterval through TakeDamage (UE: CausePainTo). */
	virtual void CausePainTo(AActor* Other);

	/** Starts the pain timer (UE). */
	void BeginPlay() override;

	/** One pain tick to every pawn inside, while bPainCausing (UE: PainTimer). */
	void PainTimer();

private:
	/** The looping pain timer (UE: TimerHandle_PainTimer). */
	FTimerHandle TimerHandle_PainTimer;
};
