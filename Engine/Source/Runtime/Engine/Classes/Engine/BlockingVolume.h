#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "BlockingVolume.generated.h"

/**
 * An invisible wall (UE: ABlockingVolume): its brush box is a static body of the physics scene. The collision is on by
 * default (UE's BlockAll profile); a map saves the brush's settings.
 */
UCLASS()
class ENGINE_API ABlockingVolume : public AVolume
{
	GENERATED_BODY()

public:
	ABlockingVolume(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
