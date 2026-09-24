#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Math/Transform.h"
#include "Components/ActorComponent.h"
#include <vector>


class Actor;

/// Unreal-like USceneComponent: ActorComponent + relative TRS + parent/child attach tree.
/// World transform: root uses owning Actor location/yaw + relative; children compose parent *
/// relative.
class SceneComponent : public ActorComponent {
public:
    SceneComponent() = default;
    ~SceneComponent() override;

    SceneComponent(const SceneComponent&) = delete;
    SceneComponent& operator=(const SceneComponent&) = delete;
    SceneComponent(SceneComponent&&) = delete;
    SceneComponent& operator=(SceneComponent&&) = delete;

    glm::vec3 RelativeLocation{0.0f};
    glm::vec3 RelativeRotation{0.0f}; // XYZ Euler, degrees
    glm::vec3 RelativeScale{1.0f};

    /// Attach under `parent`. Returns false if parent is null, this, or would create a cycle.
    [[nodiscard]] bool AttachToComponent(SceneComponent* parent, bool keepWorldTransform = false);
    void DetachFromParent(bool keepWorldTransform = false);

    [[nodiscard]] SceneComponent* GetAttachParent() const { return parent_; }
    [[nodiscard]] const std::vector<SceneComponent*>& GetAttachChildren() const {
        return children_;
    }

    [[nodiscard]] Transform GetRelativeTransform() const;
    /// Component-to-world matrix (Unreal GetComponentTransform).
    [[nodiscard]] glm::mat4 GetComponentTransform() const;
    [[nodiscard]] glm::vec3 GetComponentLocation() const;

    /// Detach attach tree, then unregister from owner.
    void DestroyComponent() override;

private:
    void detachChild(SceneComponent* child);
    [[nodiscard]] bool wouldCreateCycle(const SceneComponent* candidateParent) const;

    SceneComponent* parent_ = nullptr;
    std::vector<SceneComponent*> children_;
};

