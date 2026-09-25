#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TargetPoint.generated.h"

/**
 * A named point in a level (UE: ATargetPoint): only a transform and Tags (plan decision D15: the game meaning of
 * a point is in its tags).
 */
UCLASS()
class ENGINE_API ATargetPoint : public AActor
{
	GENERATED_BODY()

public:
	ATargetPoint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
