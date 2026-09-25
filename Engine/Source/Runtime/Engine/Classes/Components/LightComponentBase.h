#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "LightComponentBase.generated.h"

/**
 * What every light shares (UE: ULightComponentBase): brightness, colour and whether it casts shadows. LightColor is a
 * linear FLinearColor, not UE's 8-bit FColor, so the values the `.llev` files store reach the renderer exactly.
 */
UCLASS(Abstract)
class ENGINE_API ULightComponentBase : public USceneComponent
{
	GENERATED_BODY()

public:
	ULightComponentBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Brightness multiplier (UE: Intensity; Leon's lights have no photometric units). */
	UPROPERTY()
	float Intensity = 1.0f;

	/** Linear colour, alpha unused (UE: LightColor, an FColor there). */
	UPROPERTY()
	FLinearColor LightColor = FLinearColor::White;

	/** Whether the light casts shadows (UE: CastShadows); Leon's renderer shadows the first directional light only. */
	UPROPERTY()
	uint8 CastShadows : 1;

	/** UE: SetIntensity / SetLightColor / SetCastShadows; a registered light's proxy is recreated. */
	void SetIntensity(float NewIntensity);
	void SetLightColor(const FLinearColor& NewLightColor);
	void SetCastShadows(bool bNewValue);
};
