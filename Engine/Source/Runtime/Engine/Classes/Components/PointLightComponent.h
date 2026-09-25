#pragma once

#include "Components/LocalLightComponent.h"
#include "CoreMinimal.h"
#include "PointLightComponent.generated.h"

/** A light that shines in every direction from its location (UE: UPointLightComponent). */
UCLASS()
class ENGINE_API UPointLightComponent : public ULocalLightComponent
{
	GENERATED_BODY()

public:
	UPointLightComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
