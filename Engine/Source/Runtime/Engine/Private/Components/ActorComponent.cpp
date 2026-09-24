#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"


UActorComponent::~UActorComponent() {
    DestroyComponent();
}

void UActorComponent::DestroyComponent() {
    if (registered_ && owner_ != nullptr) {
        owner_->UnregisterComponent(this);
    }
    registered_ = false;
    owner_ = nullptr;
}

