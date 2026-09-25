#pragma once

#include "CoreMinimal.h"
#include "GameFramework/MovementComponent.h"
#include "BobbingMovementComponent.generated.h"

/**
 * Bobs the updated component up and down (Leon; UE has no counterpart: UInterpToMovementComponent moves between
 * control points at a constant speed, not along a sine). Each tick the component's world height is
 * BaseZ + Amplitude * (0.5 + 0.5 * sin(Speed * Time)), Time counting the seconds it ticked; X and Y are left alone.
 *
 * The legacy levels' bobbing meshes use it (P15), with the legacy level player's formula.
 */
UCLASS()
class ENGINE_API UBobbingMovementComponent : public UMovementComponent
{
	GENERATED_BODY()

public:
	UBobbingMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The lowest height, cm (world Z). */
	UPROPERTY()
	float BaseZ = 0.0f;

	/** How far above BaseZ the component rises, cm. */
	UPROPERTY()
	float Amplitude = 10.0f;

	/** Radians per second of the sine. */
	UPROPERTY()
	float Speed = 1.0f;

	void TickComponent(float DeltaTime) override;

	/** Seconds the component has ticked. */
	[[nodiscard]] float GetTime() const
	{
		return Time;
	}

private:
	float Time = 0.0f;
};
