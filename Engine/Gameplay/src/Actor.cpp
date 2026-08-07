#include <algorithm>
#include <leon/gameplay/Actor.h>
#include <leon/gameplay/ActorComponent.h>

namespace leon {

Actor::~Actor() {
    // Members (root, Character mesh, …) destroy after this body. Clear registry first so
    // component dtors do not touch a destroyed `components_` vector.
    for (ActorComponent* component : components_) {
        if (component != nullptr) {
            component->registered_ = false;
            component->owner_ = nullptr;
        }
    }
    components_.clear();
    ownedComponents_.clear();
}

void Actor::RegisterComponent(ActorComponent* component) {
    if (component == nullptr || component->registered_) {
        return;
    }
    component->SetOwner(this);
    component->registered_ = true;
    components_.push_back(component);
    // CreateDefaultSubobject after SpawnActor: match Unreal late-register BeginPlay.
    if (hasBegunPlay_) {
        component->BeginPlay();
    }
}

void Actor::UnregisterComponent(ActorComponent* component) {
    if (component == nullptr) {
        return;
    }
    components_.erase(std::remove(components_.begin(), components_.end(), component),
                      components_.end());
    component->registered_ = false;
}

void Actor::BeginPlayComponents() {
    hasBegunPlay_ = true;
    for (ActorComponent* component : components_) {
        if (component != nullptr) {
            component->BeginPlay();
        }
    }
}

void Actor::EndPlayComponents() {
    for (ActorComponent* component : components_) {
        if (component != nullptr) {
            component->EndPlay();
        }
    }
    hasBegunPlay_ = false;
}

void Actor::TickComponents(float deltaTime) {
    for (ActorComponent* component : components_) {
        if (component != nullptr && component->IsComponentTickEnabled()) {
            component->TickComponent(deltaTime);
        }
    }
}

} // namespace leon
