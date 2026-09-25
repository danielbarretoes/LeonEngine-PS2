#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "TriggerVolume.generated.h"

/**
 * A volume gameplay code tests actors against (UE: ATriggerVolume). Its Tags carry the game meaning (plan decision
 * D15: `BombSite` + `A`, `BuyZone` + `CT`). It has no body in the physics scene: overlaps are tested against the
 * brush box (AVolume::EncompassesPoint).
 */
UCLASS()
class ENGINE_API ATriggerVolume : public AVolume
{
	GENERATED_BODY()

public:
	ATriggerVolume(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
