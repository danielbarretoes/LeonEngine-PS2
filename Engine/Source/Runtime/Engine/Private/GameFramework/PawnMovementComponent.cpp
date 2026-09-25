#include "GameFramework/PawnMovementComponent.h"

#include "GameFramework/Pawn.h"

UPawnMovementComponent::UPawnMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPawnMovementComponent::PostInitProperties()
{
	Super::PostInitProperties();
	PawnOwner = Cast<APawn>(GetOwner());
}

void UPawnMovementComponent::AddInputVector(FVector WorldVector, bool bForce)
{
	if (PawnOwner != nullptr)
	{
		PawnOwner->Internal_AddMovementInput(WorldVector, bForce);
	}
}

FVector UPawnMovementComponent::GetPendingInputVector() const
{
	return PawnOwner != nullptr ? PawnOwner->GetPendingMovementInputVector() : FVector::ZeroVector;
}

FVector UPawnMovementComponent::ConsumeInputVector()
{
	return PawnOwner != nullptr ? PawnOwner->Internal_ConsumeMovementInputVector() : FVector::ZeroVector;
}
