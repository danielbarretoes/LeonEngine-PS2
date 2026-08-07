#include "FurytoonCharacter.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <leon/Engine.h>
#include <leon/level/BasicShape.h>
#include <leon/level/Level.h>

namespace game {

FurytoonCharacter::FurytoonCharacter() {
    leon::CapsuleShape capsule{};
    capsule.radius = 0.4f;
    capsule.height = 1.7f;
    SetCapsule(capsule);

    leon::CharacterMovement movement{};
    movement.MaxWalkSpeed = 6.5f;
    movement.JumpZVelocity = 8.5f;
    movement.MaxJumpCount = 2;
    movement.AirControl = 0.55f;
    movement.WalkBounds = 32.0f;
    movement.ModelYawOffsetDegrees = 0.0f;
    movement.TurnSharpness = 18.0f;
    movement.Gravity = 26.0f;
    movement.FloorY = 0.0f;
    movement.MaxStepHeight = 0.4f;
    movement.PushStrength = 1.1f;
    SetCharacterMovement(movement);

    bOrientRotationToMovement = true;

    // Kept for net boom fields / move helpers; match camera is shared top-down.
    springArm_.SetOwner(this);
    (void)springArm_.AttachToComponent(&GetRootComponent());
    springArm_.TargetArmLength = 0.0f;
    springArm_.BoomYawDegrees = 0.0f;
    springArm_.BoomPitchDegrees = -70.0f;
    springArm_.bEnableCameraLag = false;
    springArm_.bDoCollisionTest = false;
    SetMaxHealth(kMaxHealth);
    SetHealth(kMaxHealth);
}

void FurytoonCharacter::Revive(float NewHealth) {
    Character::Revive(NewHealth);
    attackKind_ = EFurytoonAttack::None;
    attackTimer_ = 0.0f;
    comboStep_ = 0;
    comboWindowRemaining_ = 0.0f;
}

float FurytoonCharacter::TakeDamage(float DamageAmount) {
    const float applied = Character::TakeDamage(DamageAmount);
    if (!IsAlive()) {
        attackKind_ = EFurytoonAttack::None;
    }
    return applied;
}

void FurytoonCharacter::EnsurePrimitiveMeshes(leon::Engine& engine) {
    if (bodyMeshIndex_ != kInvalidMesh) {
        return;
    }
    leon::Level& level = engine.GetLevel();
    leon::ResourceCache& resources = engine.GetResources();

    leon::Material mat{};
    mat.albedo = accentColor_;
    mat.shininess = 28.0f;
    mat.syncRoughnessFromShininess();

    {
        leon::Transform xf{};
        xf.scale = {0.55f, 0.95f, 0.4f};
        leon::StaticMeshComponent body =
            leon::BasicShape::cube(xf, mat, true).MakeStaticMesh(resources);
        body.mobility = leon::EComponentMobility::Movable;
        body.collisionEnabled = false;
        body.simulatePhysics = false;
        body.materialOverride = true;
        body.tag = "FurytoonBody";
        body.editorClass = "Cube";
        level.AddStaticMesh(std::move(body));
        bodyMeshIndex_ = level.StaticMeshes().size() - 1;
    }
    {
        leon::Transform xf{};
        xf.scale = {0.38f, 0.38f, 0.38f};
        leon::StaticMeshComponent head =
            leon::BasicShape::sphere(xf, mat, true, 16, 12).MakeStaticMesh(resources);
        head.mobility = leon::EComponentMobility::Movable;
        head.collisionEnabled = false;
        head.simulatePhysics = false;
        head.materialOverride = true;
        head.tag = "FurytoonHead";
        head.editorClass = "Sphere";
        level.AddStaticMesh(std::move(head));
        headMeshIndex_ = level.StaticMeshes().size() - 1;
    }
}

void FurytoonCharacter::SyncPrimitiveMeshes(leon::Level& level) const {
    auto& meshes = level.StaticMeshes();
    const bool show = IsAlive() && !IsPendingKillPending();
    const glm::vec3 feet = GetActorLocation();
    const float yaw = GetActorYaw();
    const float attackPulse = IsAttacking() ? 1.15f : 1.0f;

    if (bodyMeshIndex_ < meshes.size()) {
        leon::StaticMeshComponent& body = meshes[bodyMeshIndex_];
        body.hidden = !show;
        body.material.albedo = accentColor_;
        body.materialOverride = true;
        body.transform.position = feet + glm::vec3{0.0f, 0.55f * attackPulse, 0.0f};
        body.transform.rotationDegrees = {0.0f, yaw, 0.0f};
        body.transform.scale = {0.55f * attackPulse, 0.95f, 0.4f * attackPulse};
    }
    if (headMeshIndex_ < meshes.size()) {
        leon::StaticMeshComponent& head = meshes[headMeshIndex_];
        head.hidden = !show;
        head.material.albedo = accentColor_ * 1.1f;
        head.materialOverride = true;
        head.transform.position = feet + glm::vec3{0.0f, 1.35f * attackPulse, 0.0f};
        head.transform.rotationDegrees = {0.0f, yaw, 0.0f};
        head.transform.scale = {0.38f, 0.38f, 0.38f};
    }
}

void FurytoonCharacter::DestroyPrimitiveMeshes(leon::Level& level) {
    auto& meshes = level.StaticMeshes();
    if (bodyMeshIndex_ < meshes.size()) {
        meshes[bodyMeshIndex_].hidden = true;
        meshes[bodyMeshIndex_].mesh.reset();
    }
    if (headMeshIndex_ < meshes.size()) {
        meshes[headMeshIndex_].hidden = true;
        meshes[headMeshIndex_].mesh.reset();
    }
    bodyMeshIndex_ = kInvalidMesh;
    headMeshIndex_ = kInvalidMesh;
}

bool FurytoonCharacter::TryStartAttack(EFurytoonAttack kind) {
    if (!IsAlive() || kind == EFurytoonAttack::None || IsAttacking()) {
        return false;
    }
    attackKind_ = kind;
    attackTimer_ = 0.0f;
    hitWindowFired_ = false;
    if (kind == EFurytoonAttack::Light) {
        attackDuration_ = 0.28f;
        hitWindowAt_ = 0.08f;
        if (comboWindowRemaining_ > 0.0f) {
            comboStep_ = std::min(comboStep_ + 1, 2);
        } else {
            comboStep_ = 0;
        }
        comboWindowRemaining_ = 0.45f;
    } else {
        attackDuration_ = 0.42f;
        hitWindowAt_ = 0.14f;
        comboStep_ = 2;
        comboWindowRemaining_ = 0.0f;
    }
    return true;
}

void FurytoonCharacter::TickCombat(float deltaTime) {
    if (comboWindowRemaining_ > 0.0f) {
        comboWindowRemaining_ = std::max(0.0f, comboWindowRemaining_ - deltaTime);
        if (comboWindowRemaining_ <= 0.0f) {
            comboStep_ = 0;
        }
    }
    if (attackKind_ == EFurytoonAttack::None) {
        return;
    }
    attackTimer_ += deltaTime;
    if (attackTimer_ >= attackDuration_) {
        attackKind_ = EFurytoonAttack::None;
        attackTimer_ = 0.0f;
        hitWindowFired_ = false;
    }
}

bool FurytoonCharacter::ConsumeHitWindow() {
    if (attackKind_ == EFurytoonAttack::None || hitWindowFired_) {
        return false;
    }
    if (attackTimer_ < hitWindowAt_) {
        return false;
    }
    hitWindowFired_ = true;
    return true;
}

float FurytoonCharacter::GetAttackReach() const {
    if (attackKind_ == EFurytoonAttack::Heavy || comboStep_ >= 2) {
        return kHeavyReach;
    }
    return kLightReach;
}

float FurytoonCharacter::GetAttackDamage() const {
    float dmg = (attackKind_ == EFurytoonAttack::Heavy) ? kHeavyDamage : kLightDamage;
    if (comboStep_ >= 2 && attackKind_ == EFurytoonAttack::Light) {
        dmg *= 1.25f;
    }
    return dmg;
}

void FurytoonCharacter::ApplyHitReaction(const glm::vec3& fromFeet, float knockback) {
    glm::vec3 dir = GetActorLocation() - fromFeet;
    dir.y = 0.0f;
    if (glm::dot(dir, dir) < 1.0e-6f) {
        return;
    }
    dir = glm::normalize(dir);
    glm::vec3 feet = GetActorLocation();
    feet += dir * knockback;
    SetActorLocation(feet);
}

} // namespace game
