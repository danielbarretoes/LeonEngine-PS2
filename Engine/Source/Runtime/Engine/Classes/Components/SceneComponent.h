#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Math/Transform.h"
#include "Components/ActorComponent.h"
#include <vector>


class AActor;

/// Unreal-like USceneComponent: UActorComponent + relative TRS + parent/child attach tree.
/// World transform: root uses owning Actor location/yaw + relative; children compose parent *
/// relative.
class USceneComponent : public UActorComponent {
public:
    USceneComponent() = default;
    ~USceneComponent() override;

    USceneComponent(const USceneComponent&) = delete;
    USceneComponent& operator=(const USceneComponent&) = delete;
    USceneComponent(USceneComponent&&) = delete;
    USceneComponent& operator=(USceneComponent&&) = delete;

    glm::vec3 RelativeLocation{0.0f};
    glm::vec3 RelativeRotation{0.0f}; // XYZ Euler, degrees
    glm::vec3 RelativeScale{1.0f};

    /// Attach under `parent`. Returns false if parent is null, this, or would create a cycle.
    [[nodiscard]] bool AttachToComponent(USceneComponent* parent, bool keepWorldTransform = false);
    void DetachFromParent(bool keepWorldTransform = false);

    [[nodiscard]] USceneComponent* GetAttachParent() const { return parent_; }
    [[nodiscard]] const std::vector<USceneComponent*>& GetAttachChildren() const {
        return children_;
    }

    [[nodiscard]] FTransform GetRelativeTransform() const;
    /// Component-to-world matrix (Unreal GetComponentTransform).
    [[nodiscard]] glm::mat4 GetComponentTransform() const;
    [[nodiscard]] glm::vec3 GetComponentLocation() const;

    /// Detach attach tree, then unregister from owner.
    void DestroyComponent() override;

private:
    void detachChild(USceneComponent* child);
    [[nodiscard]] bool wouldCreateCycle(const USceneComponent* candidateParent) const;

    USceneComponent* parent_ = nullptr;
    std::vector<USceneComponent*> children_;
};

