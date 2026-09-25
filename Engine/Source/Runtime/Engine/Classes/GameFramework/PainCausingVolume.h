#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "PainCausingVolume.generated.h"

/**
 * A volume that damages the characters inside it (UE: APainCausingVolume, an APhysicsVolume there; Leon has no physics
 * volumes). The damage is DamagePerSec * PainInterval every PainInterval seconds while bPainCausing; gameplay code
 * applies it (VolumeHelpers.h).
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
};
