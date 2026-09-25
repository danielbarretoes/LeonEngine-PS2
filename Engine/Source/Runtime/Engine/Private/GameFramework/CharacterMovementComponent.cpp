#include "GameFramework/CharacterMovementComponent.h"

#include "GameFramework/Character.h"

UCharacterMovementComponent::UCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCharacterMovementComponent::PostInitProperties()
{
	Super::PostInitProperties();
	CharacterOwner = Cast<ACharacter>(GetOwner());
}
