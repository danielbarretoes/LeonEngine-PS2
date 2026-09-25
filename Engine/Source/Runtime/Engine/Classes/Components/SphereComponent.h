#pragma once

#include "Components/ShapeComponent.h"
#include "CoreMinimal.h"
#include "SphereComponent.generated.h"

/** A sphere (UE: USphereComponent). */
UCLASS()
class ENGINE_API USphereComponent : public UShapeComponent
{
	GENERATED_BODY()

public:
	USphereComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Radius, cm (UE: SetSphereRadius; InitSphereRadius before registration). */
	void SetSphereRadius(float InSphereRadius)
	{
		SphereRadius = InSphereRadius;
	}
	void InitSphereRadius(float InSphereRadius)
	{
		SphereRadius = InSphereRadius;
	}
	[[nodiscard]] float GetUnscaledSphereRadius() const
	{
		return SphereRadius;
	}
	[[nodiscard]] float GetScaledSphereRadius() const
	{
		return SphereRadius * GetShapeScale();
	}

	[[nodiscard]] FCollisionShape GetCollisionShape(float Inflation = 0.0f) const override;

private:
	/** Radius, cm (UE: SphereRadius). */
	UPROPERTY()
	float SphereRadius = 32.0f;
};
