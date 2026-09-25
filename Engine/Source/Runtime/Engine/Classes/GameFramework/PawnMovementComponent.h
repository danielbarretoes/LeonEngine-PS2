#pragma once

#include "CoreMinimal.h"
#include "GameFramework/MovementComponent.h"
#include "PawnMovementComponent.generated.h"

class APawn;

/** Movement of a pawn (UE: UPawnMovementComponent; UE puts UNavMovementComponent between the two). */
UCLASS(Abstract)
class ENGINE_API UPawnMovementComponent : public UMovementComponent
{
	GENERATED_BODY()

public:
	UPawnMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The pawn that owns the component (UE: GetPawnOwner). */
	[[nodiscard]] APawn* GetPawnOwner() const
	{
		return PawnOwner;
	}

	void PostInitProperties() override;

	/** Adds to the owning pawn's input vector for this frame (UE: AddInputVector). */
	virtual void AddInputVector(FVector WorldVector, bool bForce = false);

	/** The pawn's pending input vector (UE: GetPendingInputVector). */
	[[nodiscard]] FVector GetPendingInputVector() const;

	/** Returns the pawn's input vector and clears it for the next frame (UE: ConsumeInputVector). */
	virtual FVector ConsumeInputVector();

protected:
	/** The owning pawn (UE: PawnOwner). */
	UPROPERTY(Transient)
	APawn* PawnOwner = nullptr;
};
