#pragma once

#include "CoreMinimal.h"
#include "GameFramework/MovementComponent.h"
#include "RotatingMovementComponent.generated.h"

/**
 * Turns the updated component at a constant rate, optionally about a pivot (UE: URotatingMovementComponent). Each tick
 * the rotation turns by RotationRate * DeltaTime, in the component's local space or in world space; a non-zero
 * PivotTranslation (in the component's local space) makes the component orbit that pivot as it turns. Leon moves the
 * component without sweeping (UE's MoveUpdatedComponent without collision, as the rotating movement does).
 *
 * The legacy levels' spinning meshes use it (P15): a yaw rate in world space.
 */
UCLASS()
class ENGINE_API URotatingMovementComponent : public UMovementComponent
{
	GENERATED_BODY()

public:
	URotatingMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Degrees per second about each axis (UE: RotationRate; the default turns half a circle per second in yaw). */
	UPROPERTY()
	FRotator RotationRate = FRotator(0.0f, 180.0f, 0.0f);

	/** The pivot the component turns about, relative to it in its local space (UE: PivotTranslation). */
	UPROPERTY()
	FVector PivotTranslation = FVector::ZeroVector;

	/** Rotate in the component's local space; world space otherwise (UE: bRotationInLocalSpace). */
	UPROPERTY()
	bool bRotationInLocalSpace = true;

	void TickComponent(float DeltaTime) override;
};
