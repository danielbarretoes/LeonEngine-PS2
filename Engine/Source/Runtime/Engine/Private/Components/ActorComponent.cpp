#include <leon/gameplay/Actor.h>
#include <leon/gameplay/ActorComponent.h>

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
