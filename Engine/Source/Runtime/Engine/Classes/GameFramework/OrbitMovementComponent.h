#pragma once

#include "CoreMinimal.h"
#include "GameFramework/MovementComponent.h"
#include "OrbitMovementComponent.generated.h"

/**
 * Moves the updated component around a vertical axis through the world origin (Leon; UE would combine a
 * URotatingMovementComponent with a PivotTranslation and a height animation). Each tick, with A = Speed * Time (Time
 * counting the seconds it ticked), the component's world location is (Radius * cos A, Radius * sin A,
 * Height + HeightAmplitude * sin 2A): it starts on +X and turns toward +Y.
 *
 * The legacy levels' orbiting point lights use it (P15), with the legacy level player's formula.
 */
UCLASS()
class ENGINE_API UOrbitMovementComponent : public UMovementComponent
{
	GENERATED_BODY()

public:
	UOrbitMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Distance from the vertical axis, cm. */
	UPROPERTY()
	float Radius = 100.0f;

	/** The middle height, cm (world Z). */
	UPROPERTY()
	float Height = 100.0f;

	/** How far the height swings above and below Height, cm. */
	UPROPERTY()
	float HeightAmplitude = 0.0f;

	/** Radians per second around the axis. */
	UPROPERTY()
	float Speed = 1.0f;

	void TickComponent(float DeltaTime) override;

private:
	float Time = 0.0f;
};
