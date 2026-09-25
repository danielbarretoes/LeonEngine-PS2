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
