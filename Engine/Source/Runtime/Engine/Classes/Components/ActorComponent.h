#pragma once

namespace leon {

class Actor;

/// Unreal-like UActorComponent (no U-prefix): non-transform logic + tick hooks.
///
/// Contract (Leon, not full UE):
/// - Prefer **member** components (`RegisterComponent`) for defaults (Character mesh, root).
/// - Use `CreateDefaultSubobject<T>()` for heap-owned extras on the Actor.
/// - `Level::StaticMeshComponent` remains a level POD — not an ActorComponent.
/// - No reflection / Blueprint; no CreateDefaultSubobject name registry.
class ActorComponent {
public:
    ActorComponent() = default;
    virtual ~ActorComponent();

    ActorComponent(const ActorComponent&) = delete;
    ActorComponent& operator=(const ActorComponent&) = delete;
    ActorComponent(ActorComponent&&) = delete;
    ActorComponent& operator=(ActorComponent&&) = delete;

    void SetOwner(Actor* owner) { owner_ = owner; }
    [[nodiscard]] Actor* GetOwner() const { return owner_; }

    [[nodiscard]] bool IsRegistered() const { return registered_; }
    [[nodiscard]] bool IsComponentTickEnabled() const { return primaryTickEnabled_; }
    void SetComponentTickEnabled(bool enabled) { primaryTickEnabled_ = enabled; }

    virtual void BeginPlay() {}
    virtual void EndPlay() {}
    /// Only called when `IsComponentTickEnabled()` (off by default).
    virtual void TickComponent(float /*deltaTime*/) {}

    /// Unregister from owner. Subclasses may detach scene links first.
    virtual void DestroyComponent();

protected:
    friend class Actor;

    Actor* owner_ = nullptr;
    bool registered_ = false;
    bool primaryTickEnabled_ = false;
};

} // namespace leon
