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
class ENGINE_API USceneComponent : public UActorComponent {
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
    [[nodiscard]] bool AttachToComponent(USceneComponent* InParent, bool bKeepWorldTransform = false);
    void DetachFromParent(bool bKeepWorldTransform = false);

    [[nodiscard]] USceneComponent* GetAttachParent() const { return Parent; }
    [[nodiscard]] const std::vector<USceneComponent*>& GetAttachChildren() const {
        return Children;
    }

    [[nodiscard]] FTransform GetRelativeTransform() const;
    /// Component-to-world matrix (Unreal GetComponentTransform).
    [[nodiscard]] glm::mat4 GetComponentTransform() const;
    [[nodiscard]] glm::vec3 GetComponentLocation() const;

    /// Detach attach tree, then unregister from owner.
    void DestroyComponent() override;

private:
    void DetachChild(USceneComponent* Child);
    [[nodiscard]] bool WouldCreateCycle(const USceneComponent* CandidateParent) const;

    USceneComponent* Parent = nullptr;
    std::vector<USceneComponent*> Children;
};

