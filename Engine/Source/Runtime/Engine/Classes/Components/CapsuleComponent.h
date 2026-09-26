#pragma once

#include "Components/ShapeComponent.h"
#include "CoreMinimal.h"
#include "CapsuleComponent.generated.h"

/**
 * A capsule along Z (UE: UCapsuleComponent). The half height includes the hemispherical caps. Its body in the physics
 * scene is an upright capsule, centred on the component as in UE, or standing on it with bBaseAtComponentLocation.
 */
UCLASS()
class ENGINE_API UCapsuleComponent : public UShapeComponent
{
	GENERATED_BODY()

public:
	UCapsuleComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Sets both dimensions, cm (UE: SetCapsuleSize; InitCapsuleSize before registration). */
	void SetCapsuleSize(float InRadius, float InHalfHeight);
	void InitCapsuleSize(float InRadius, float InHalfHeight)
	{
		SetCapsuleSize(InRadius, InHalfHeight);
	}
	void SetCapsuleRadius(float Radius)
	{
		SetCapsuleSize(Radius, CapsuleHalfHeight);
	}
	void SetCapsuleHalfHeight(float HalfHeight)
	{
		SetCapsuleSize(CapsuleRadius, HalfHeight);
	}

	[[nodiscard]] float GetUnscaledCapsuleRadius() const
	{
		return CapsuleRadius;
	}
	[[nodiscard]] float GetUnscaledCapsuleHalfHeight() const
	{
		return CapsuleHalfHeight;
	}
	[[nodiscard]] float GetScaledCapsuleRadius() const
	{
		return CapsuleRadius * GetShapeScale();
	}
	[[nodiscard]] float GetScaledCapsuleHalfHeight() const
	{
		return CapsuleHalfHeight * GetShapeScale();
	}

	[[nodiscard]] FCollisionShape GetCollisionShape(float Inflation = 0.0f) const override;

	/**
	 * The capsule stands on the component's location instead of being centred on it (Leon: the character's capsule,
	 * whose actor location is the feet).
	 */
	UPROPERTY()
	uint8 bBaseAtComponentLocation : 1;

private:
	/** Half the capsule's height including the caps, cm (UE: CapsuleHalfHeight). */
	UPROPERTY()
	float CapsuleHalfHeight = 44.0f;

	/** Radius, cm (UE: CapsuleRadius). */
	UPROPERTY()
	float CapsuleRadius = 22.0f;
};
