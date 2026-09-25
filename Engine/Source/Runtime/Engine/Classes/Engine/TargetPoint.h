#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TargetPoint.generated.h"

/**
 * A named point in a level (UE: ATargetPoint): only a transform and Tags. The `.llev` reader turns the legacy
 * AISpawnPoint records into target points and puts the record's tag in Tags (plan decision D15).
 */
UCLASS()
class ENGINE_API ATargetPoint : public AActor
{
	GENERATED_BODY()

public:
	ATargetPoint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
