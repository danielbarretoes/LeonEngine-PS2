#pragma once


class AActor;

/// Unreal-like UActorComponent (no U-prefix): non-transform logic + tick hooks.
///
/// Contract (Leon, not full UE):
/// - Prefer **member** components (`RegisterComponent`) for defaults (Character mesh, root).
/// - Use `CreateDefaultSubobject<T>()` for heap-owned extras on the Actor.
/// - `Level::StaticMeshComponent` remains a level POD — not an UActorComponent.
/// - No reflection / Blueprint; no CreateDefaultSubobject name registry.
class UActorComponent {
public:
    UActorComponent() = default;
    virtual ~UActorComponent();

    UActorComponent(const UActorComponent&) = delete;
    UActorComponent& operator=(const UActorComponent&) = delete;
    UActorComponent(UActorComponent&&) = delete;
    UActorComponent& operator=(UActorComponent&&) = delete;

    void SetOwner(AActor* owner) { owner_ = owner; }
    [[nodiscard]] AActor* GetOwner() const { return owner_; }

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
    friend class AActor;

    AActor* owner_ = nullptr;
    bool registered_ = false;
    bool primaryTickEnabled_ = false;
};

