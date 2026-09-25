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

protected:
	/** The owning pawn (UE: PawnOwner). */
	UPROPERTY(Transient)
	APawn* PawnOwner = nullptr;
};
