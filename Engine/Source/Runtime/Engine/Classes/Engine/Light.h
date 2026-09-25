#pragma once

#include "Components/LightComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Light.generated.h"

/** A placed light (UE: ALight): its root is the light component, which the subclass creates. */
UCLASS(Abstract)
class ENGINE_API ALight : public AActor
{
	GENERATED_BODY()

public:
	ALight(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: GetLightComponent. */
	[[nodiscard]] ULightComponent* GetLightComponent() const
	{
		return LightComponent;
	}

protected:
	/** The root light (UE: LightComponent). */
	UPROPERTY()
	ULightComponent* LightComponent = nullptr;
};
