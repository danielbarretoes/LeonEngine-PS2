#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

namespace {

[[nodiscard]] Transform decomposeApprox(const glm::mat4& m) {
    Transform t{};
    t.position = glm::vec3(m[3]);
    t.scale.x = glm::length(glm::vec3(m[0]));
    t.scale.y = glm::length(glm::vec3(m[1]));
    t.scale.z = glm::length(glm::vec3(m[2]));
    constexpr float kEps = 1.0e-6f;
    const glm::vec3 col0 =
        t.scale.x > kEps ? glm::vec3(m[0]) / t.scale.x : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 col1 =
        t.scale.y > kEps ? glm::vec3(m[1]) / t.scale.y : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 col2 =
        t.scale.z > kEps ? glm::vec3(m[2]) / t.scale.z : glm::vec3(0.0f, 0.0f, 1.0f);
    // XYZ Euler extraction (degrees) matching Transform::modelMatrix order Rx*Ry*Rz.
    t.rotationDegrees.y = std::atan2(-col0.z, col2.z) * (180.0f / 3.14159265358979323846f);
    t.rotationDegrees.x =
        std::asin(std::clamp(col1.z, -1.0f, 1.0f)) * (180.0f / 3.14159265358979323846f);
    t.rotationDegrees.z = std::atan2(-col1.x, col1.y) * (180.0f / 3.14159265358979323846f);
    (void)col2;
    return t;
}

} // namespace

SceneComponent::~SceneComponent() {
    // ActorComponent dtor also calls DestroyComponent; detach scene links first while owner may
    // still be valid (Actor::~ clears owner before member SceneComponent dtors).
    while (!children_.empty()) {
        SceneComponent* child = children_.back();
        child->DetachFromParent(false);
    }
    DetachFromParent(false);
}

Transform SceneComponent::GetRelativeTransform() const {
    Transform t{};
    t.position = RelativeLocation;
    t.rotationDegrees = RelativeRotation;
    t.scale = RelativeScale;
    return t;
}

bool SceneComponent::wouldCreateCycle(const SceneComponent* candidateParent) const {
    for (const SceneComponent* walk = candidateParent; walk != nullptr; walk = walk->parent_) {
        if (walk == this) {
            return true;
        }
    }
    return false;
}

void SceneComponent::detachChild(SceneComponent* child) {
    children_.erase(std::remove(children_.begin(), children_.end(), child), children_.end());
}

bool SceneComponent::AttachToComponent(SceneComponent* parent, bool keepWorldTransform) {
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
        const Transform relative = decomposeApprox(parentInv * worldBefore);
        RelativeLocation = relative.position;
        RelativeRotation = relative.rotationDegrees;
        RelativeScale = relative.scale;
    }
    return true;
}

void SceneComponent::DetachFromParent(bool keepWorldTransform) {
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
        const Transform world = decomposeApprox(worldBefore);
        RelativeLocation = world.position;
        RelativeRotation = world.rotationDegrees;
        RelativeScale = world.scale;
        if (owner_ != nullptr) {
            RelativeLocation -= owner_->GetActorLocation();
            RelativeRotation.y -= owner_->GetActorYaw();
        }
    }
}

glm::mat4 SceneComponent::GetComponentTransform() const {
    const Transform relative = GetRelativeTransform();
    if (parent_ != nullptr) {
        return parent_->GetComponentTransform() * relative.modelMatrix();
    }
    if (owner_ != nullptr) {
        Transform world{};
        world.position = owner_->GetActorLocation() + RelativeLocation;
        world.rotationDegrees = RelativeRotation;
        world.rotationDegrees.y += owner_->GetActorYaw();
        world.scale = RelativeScale;
        return world.modelMatrix();
    }
    return relative.modelMatrix();
}

glm::vec3 SceneComponent::GetComponentLocation() const {
    return glm::vec3(GetComponentTransform()[3]);
}

void SceneComponent::DestroyComponent() {
    while (!children_.empty()) {
        SceneComponent* child = children_.back();
        child->DetachFromParent(false);
    }
    DetachFromParent(false);
    ActorComponent::DestroyComponent();
}

