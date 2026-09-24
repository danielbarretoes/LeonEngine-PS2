#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

namespace leon {

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

} // namespace leon
