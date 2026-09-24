#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"


UActorComponent::~UActorComponent() {
    DestroyComponent();
}

void UActorComponent::DestroyComponent() {
    if (bRegistered && Owner != nullptr) {
        Owner->UnregisterComponent(this);
    }
    bRegistered = false;
    Owner = nullptr;
}

