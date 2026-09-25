#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Info.generated.h"

/**
 * An actor that holds information rather than being placed in the world (UE: AInfo): the base of the game mode, the
 * game state and the player state. It is hidden and has no transform to speak of.
 */
UCLASS(Abstract, NotPlaceable)
class ENGINE_API AInfo : public AActor
{
	GENERATED_BODY()

public:
	AInfo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
