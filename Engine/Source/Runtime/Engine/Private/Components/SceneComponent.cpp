#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

namespace {

[[nodiscard]] FTransform decomposeApprox(const glm::mat4& m) {
    FTransform t{};
    t.Position = glm::vec3(m[3]);
    t.Scale.x = glm::length(glm::vec3(m[0]));
    t.Scale.y = glm::length(glm::vec3(m[1]));
    t.Scale.z = glm::length(glm::vec3(m[2]));
    constexpr float kEps = 1.0e-6f;
    const glm::vec3 col0 =
        t.Scale.x > kEps ? glm::vec3(m[0]) / t.Scale.x : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 col1 =
        t.Scale.y > kEps ? glm::vec3(m[1]) / t.Scale.y : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 col2 =
        t.Scale.z > kEps ? glm::vec3(m[2]) / t.Scale.z : glm::vec3(0.0f, 0.0f, 1.0f);
    // XYZ Euler extraction (degrees) matching FTransform::modelMatrix order Rx*Ry*Rz.
    t.RotationDegrees.y = std::atan2(-col0.z, col2.z) * (180.0f / 3.14159265358979323846f);
    t.RotationDegrees.x =
        std::asin(std::clamp(col1.z, -1.0f, 1.0f)) * (180.0f / 3.14159265358979323846f);
    t.RotationDegrees.z = std::atan2(-col1.x, col1.y) * (180.0f / 3.14159265358979323846f);
    (void)col2;
    return t;
}

} // namespace

USceneComponent::~USceneComponent() {
    // UActorComponent dtor also calls DestroyComponent; detach scene links first while owner may
    // still be valid (Actor::~ clears owner before member USceneComponent dtors).
    while (!children_.empty()) {
        USceneComponent* child = children_.back();
        child->DetachFromParent(false);
    }
    DetachFromParent(false);
}

FTransform USceneComponent::GetRelativeTransform() const {
    FTransform t{};
    t.Position = RelativeLocation;
    t.RotationDegrees = RelativeRotation;
    t.Scale = RelativeScale;
    return t;
}

bool USceneComponent::wouldCreateCycle(const USceneComponent* candidateParent) const {
    for (const USceneComponent* walk = candidateParent; walk != nullptr; walk = walk->parent_) {
        if (walk == this) {
            return true;
        }
    }
    return false;
}

void USceneComponent::detachChild(USceneComponent* child) {
    children_.erase(std::remove(children_.begin(), children_.end(), child), children_.end());
}

bool USceneComponent::AttachToComponent(USceneComponent* parent, bool keepWorldTransform) {
    if (parent == nullptr || parent == this || wouldCreateCycle(parent)) {
        return false;
    }

    glm::mat4 worldBefore{};
    if (keepWorldTransform) {
        worldBefore = GetComponentTransform();
    }

    DetachFromParent(false);
    parent_ = parent;
    parent_->children_.push_back(this);
    if (owner_ == nullptr) {
        owner_ = parent->owner_;
    }

    if (keepWorldTransform) {
        const glm::mat4 parentWorld = parent_->GetComponentTransform();
        const glm::mat4 parentInv = glm::inverse(parentWorld);
        const FTransform relative = decomposeApprox(parentInv * worldBefore);
        RelativeLocation = relative.Position;
        RelativeRotation = relative.RotationDegrees;
        RelativeScale = relative.Scale;
    }
    return true;
}

void USceneComponent::DetachFromParent(bool keepWorldTransform) {
    if (parent_ == nullptr) {
        return;
    }

    glm::mat4 worldBefore{};
    if (keepWorldTransform) {
        worldBefore = GetComponentTransform();
    }

    parent_->detachChild(this);
    parent_ = nullptr;

    if (keepWorldTransform) {
        const FTransform world = decomposeApprox(worldBefore);
        RelativeLocation = world.Position;
        RelativeRotation = world.RotationDegrees;
        RelativeScale = world.Scale;
        if (owner_ != nullptr) {
            RelativeLocation -= owner_->GetActorLocation();
            RelativeRotation.y -= owner_->GetActorYaw();
        }
    }
}

glm::mat4 USceneComponent::GetComponentTransform() const {
    const FTransform relative = GetRelativeTransform();
    if (parent_ != nullptr) {
        return parent_->GetComponentTransform() * relative.ModelMatrix();
    }
    if (owner_ != nullptr) {
        FTransform world{};
        world.Position = owner_->GetActorLocation() + RelativeLocation;
        world.RotationDegrees = RelativeRotation;
        world.RotationDegrees.y += owner_->GetActorYaw();
        world.Scale = RelativeScale;
        return world.ModelMatrix();
    }
    return relative.ModelMatrix();
}

glm::vec3 USceneComponent::GetComponentLocation() const {
    return glm::vec3(GetComponentTransform()[3]);
}

void USceneComponent::DestroyComponent() {
    while (!children_.empty()) {
        USceneComponent* child = children_.back();
        child->DetachFromParent(false);
    }
    DetachFromParent(false);
    UActorComponent::DestroyComponent();
}

