#pragma once

#include <cstddef>
#include <cstdint>
#include <glm/vec3.hpp>
#include <limits>
#include <leon/Gameplay.h>
#include <leon/gameplay/SpringArmComponent.h>

namespace leon {
class Engine;
class Level;
} // namespace leon

namespace game {

enum class EFurytoonAttack : std::uint8_t {
    None = 0,
    Light,
    Heavy,
};

/// Top-down party fighter pawn: double jump + short light/heavy combos.
/// Visuals: movable Cube body + Sphere head synced into the Level each frame.
class FurytoonCharacter final : public leon::Character {
public:
    static constexpr float kMaxHealth = 100.0f;
    static constexpr float kLightDamage = 8.0f;
    static constexpr float kHeavyDamage = 16.0f;
    static constexpr float kLightReach = 1.35f;
    static constexpr float kHeavyReach = 1.55f;
    static constexpr float kAttackHalfHeight = 0.9f;
    static constexpr std::size_t kInvalidMesh = (std::numeric_limits<std::size_t>::max)();

    FurytoonCharacter();

    [[nodiscard]] leon::SpringArmComponent& SpringArm() { return springArm_; }
    [[nodiscard]] const leon::SpringArmComponent& SpringArm() const { return springArm_; }

    void SetAccentColor(const glm::vec3& accent) { accentColor_ = accent; }
    [[nodiscard]] const glm::vec3& GetAccentColor() const { return accentColor_; }

    /// Spawn Cube body + Sphere head into the Level (no PhysScene collision).
    void EnsurePrimitiveMeshes(leon::Engine& engine);
    /// Push actor pose into Level StaticMeshComponents (and hide when dead).
    void SyncPrimitiveMeshes(leon::Level& level) const;
    void DestroyPrimitiveMeshes(leon::Level& level);

    bool TryStartAttack(EFurytoonAttack kind);
    void TickCombat(float deltaTime);
    [[nodiscard]] bool IsAttacking() const { return attackKind_ != EFurytoonAttack::None; }
    [[nodiscard]] EFurytoonAttack GetAttackKind() const { return attackKind_; }
    [[nodiscard]] bool ConsumeHitWindow();
    [[nodiscard]] float GetAttackReach() const;
    [[nodiscard]] float GetAttackDamage() const;
    [[nodiscard]] int GetComboStep() const { return comboStep_; }

    void ApplyHitReaction(const glm::vec3& fromFeet, float knockback);
    float TakeDamage(float DamageAmount) override;
    void Revive(float NewHealth);

private:
    leon::SpringArmComponent springArm_{};
    glm::vec3 accentColor_{0.3f, 0.85f, 1.0f};

    std::size_t bodyMeshIndex_ = kInvalidMesh;
    std::size_t headMeshIndex_ = kInvalidMesh;

    EFurytoonAttack attackKind_ = EFurytoonAttack::None;
    float attackTimer_ = 0.0f;
    float attackDuration_ = 0.0f;
    float hitWindowAt_ = 0.0f;
    bool hitWindowFired_ = false;
    int comboStep_ = 0;
    float comboWindowRemaining_ = 0.0f;
};

} // namespace game
