#pragma once

#include "Components/LightComponentBase.h"
#include "CoreMinimal.h"
#include "LightComponent.generated.h"

/**
 * A light that illuminates the world (UE: ULightComponent). It shines along its forward axis (+X): GetDirection is the
 * unit X axis of its world transform.
 */
UCLASS(Abstract)
class ENGINE_API ULightComponent : public ULightComponentBase
{
	GENERATED_BODY()

public:
	ULightComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The direction the light travels (UE: GetDirection). */
	[[nodiscard]] FVector GetDirection() const;
};
