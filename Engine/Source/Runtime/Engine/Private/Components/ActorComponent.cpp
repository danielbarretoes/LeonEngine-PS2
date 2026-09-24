#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"


ActorComponent::~ActorComponent() {
    DestroyComponent();
}

void ActorComponent::DestroyComponent() {
    if (registered_ && owner_ != nullptr) {
        owner_->UnregisterComponent(this);
    }
    registered_ = false;
    owner_ = nullptr;
}

