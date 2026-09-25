#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "MovementComponent.generated.h"

class USceneComponent;

/**
 * Moves a scene component of its owner (UE: UMovementComponent). The updated component defaults to the owner's root,
 * set when the component registers.
 */
UCLASS(Abstract)
class ENGINE_API UMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The component this one moves (UE: UpdatedComponent). */
	UPROPERTY(Transient)
	USceneComponent* UpdatedComponent = nullptr;

	/** Current velocity, cm/s (UE: Velocity). */
	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	/** Moves another component (UE: SetUpdatedComponent). */
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);

	/** Speed in cm/s (UE: MaxSpeed of the current mode; 0 by default). */
	[[nodiscard]] virtual float GetMaxSpeed() const;

protected:
	/** Uses the owner's root component when none was set (UE: bAutoRegisterUpdatedComponent). */
	void OnRegister() override;
};
