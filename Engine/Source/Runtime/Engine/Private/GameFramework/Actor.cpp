#include <algorithm>
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"


AActor::~AActor() {
    // Members (root, Character mesh, …) destroy after this body. Clear registry first so
    // component dtors do not touch a destroyed `components_` vector.
    for (UActorComponent* component : components_) {
        if (component != nullptr) {
            component->registered_ = false;
            component->owner_ = nullptr;
        }
    }
    components_.clear();
    ownedComponents_.clear();
}

void AActor::RegisterComponent(UActorComponent* component) {
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

void AActor::UnregisterComponent(UActorComponent* component) {
    if (component == nullptr) {
        return;
    }
    components_.erase(std::remove(components_.begin(), components_.end(), component),
                      components_.end());
    component->registered_ = false;
}

void AActor::BeginPlayComponents() {
    hasBegunPlay_ = true;
    for (UActorComponent* component : components_) {
        if (component != nullptr) {
            component->BeginPlay();
        }
    }
}

void AActor::EndPlayComponents() {
    for (UActorComponent* component : components_) {
        if (component != nullptr) {
            component->EndPlay();
        }
    }
    hasBegunPlay_ = false;
}

void AActor::TickComponents(float deltaTime) {
    for (UActorComponent* component : components_) {
        if (component != nullptr && component->IsComponentTickEnabled()) {
            component->TickComponent(deltaTime);
        }
    }
}

