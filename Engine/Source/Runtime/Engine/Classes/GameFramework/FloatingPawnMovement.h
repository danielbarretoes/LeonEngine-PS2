#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "FloatingPawnMovement.generated.h"

/**
 * Flying movement for a pawn (UE: UFloatingPawnMovement): each tick it consumes the pawn's input vector and moves the
 * updated component along it.
 *
 * Leon moves at MaxSpeed along the normalized input vector, with no acceleration, deceleration or collision (the
 * legacy fly camera's feel); UE accelerates toward MaxSpeed and sweeps. Acceleration, Deceleration and TurningBoost are
 * kept for UE's shape.
 */
UCLASS()
class ENGINE_API UFloatingPawnMovement : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UFloatingPawnMovement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Speed along the input, cm/s (UE: MaxSpeed). */
	UPROPERTY()
	float MaxSpeed = 1200.0f;

	/** UE: Acceleration (Leon moves at MaxSpeed at once). */
	UPROPERTY()
	float Acceleration = 4000.0f;

	/** UE: Deceleration (Leon stops at once). */
	UPROPERTY()
	float Deceleration = 8000.0f;

	/** UE: TurningBoost. */
	UPROPERTY()
	float TurningBoost = 8.0f;

	[[nodiscard]] float GetMaxSpeed() const override
	{
		return MaxSpeed;
	}

	/** Consumes the input vector and moves (UE: TickComponent). */
	void TickComponent(float DeltaTime) override;
};
