#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQuery.h"
#include "CollisionShape.h"
#include "Physics/PhysScene.h"


class FSceneRenderer;
class FDebugDraw;

/// Unreal-like EMovementMode (CMC lite: Walking / Falling only).
enum class EMovementMode : std::uint8_t {
    None = 0,
    Walking,
    Falling,
};

/// Unreal-like FFindFloorResult (CMC floor query).
struct FindFloorResult {
    bool bBlockingHit = false;
    bool bWalkableFloor = false;
    /// Distance from capsule feet down to floor ImpactPoint.y (>= 0 when hit below/at feet).
    float FloorDist = 0.0f;
    FHitResult Hit{};
};

/// Unreal-like UCharacterMovementComponent tunables (PascalCase Unreal-like field names).
struct CharacterMovement {
    /// Unreal MaxWalkSpeed.
    float MaxWalkSpeed = 4.5f;
    /// Unreal JumpZVelocity.
    float JumpZVelocity = 7.0f;
    /// World gravity acceleration (Leon absolute; UE uses GravityScale × world gravity).
    float Gravity = 24.0f;
    float TurnSharpness = 16.0f;
    float ModelYawOffsetDegrees = 0.0f;
    float FloorY = 0.0f;
    float Skin = 0.02f;
    /// Unreal MaxStepHeight: geometric step-up + floor probe window.
    float MaxStepHeight = 0.35f;
    float WalkBounds = 18.0f;
    float PushStrength = 1.0f;
    float PushDamping = 6.0f;
    /// Unreal WalkableFloorZ (cos of max walkable slope). Default ~44° (UE).
    float WalkableFloorZ = 0.71f;
    /// Unreal AirControl [0,1]: fraction of MaxWalkSpeed applied while Falling.
    float AirControl = 0.35f;
    /// Max jumps from ground before landing (1 = normal, 2 = double jump). Projects may raise.
    int MaxJumpCount = 1;
};

/// Kinematic capsule pawn (Unreal-style ACharacter + CMC lite).
///
/// Contract:
/// - Actor location = capsule **feet** (bottom), not capsule center.
/// - Capsule extends upward by FCapsuleShape::height; XZ radius FCapsuleShape::radius.
/// - Not registered as a FPhysScene FBodyInstance; moves via PerformMovement queries.
/// - Modes: Walking / Falling via SetMovementMode; floor via FindFloor → IsWalkable.
class Character : public Pawn {
public:
    Character();

    void SetCapsule(const FCapsuleShape& capsule) { capsule_ = capsule; }
    void SetCharacterMovement(const CharacterMovement& movement) { movement_ = movement; }

    [[nodiscard]] const FCapsuleShape& GetCapsule() const { return capsule_; }
    [[nodiscard]] CharacterMovement& GetCharacterMovement() { return movement_; }
    [[nodiscard]] const CharacterMovement& GetCharacterMovement() const { return movement_; }

    /// Unreal-like UCharacterMovementComponent::SetMovementMode / MovementMode.
    void SetMovementMode(EMovementMode newMode);
    [[nodiscard]] EMovementMode GetMovementMode() const { return movementMode_; }

    /// Unreal-like UCharacterMovementComponent::IsMovingOnGround.
    [[nodiscard]] bool IsMovingOnGround() const { return movementMode_ == EMovementMode::Walking; }
    /// Unreal-like UCharacterMovementComponent::IsFalling (airborne / jumping).
    [[nodiscard]] bool IsFalling() const { return movementMode_ == EMovementMode::Falling; }
    /// Vertical velocity (Unreal Velocity.Z) for jump SM apex detection.
    [[nodiscard]] float GetVelocityZ() const { return velocityY_; }
    /// True for one frame after leaving air → ground (consumed by UAnimInstance).
    [[nodiscard]] bool ConsumeJustLanded();

    /// Last successful FindFloor from integrateVertical (may be empty if never queried).
    [[nodiscard]] const FindFloorResult& GetCurrentFloor() const { return currentFloor_; }

    /// Unreal IsWalkable: ImpactNormal.Z >= WalkableFloorZ.
    [[nodiscard]] bool IsWalkable(const FHitResult& hit) const;

    /// Unreal-like FindFloor: downward sphere trace from feet; fills outFloor.
    void FindFloor(FPhysScene& physScene, FindFloorResult& outFloor, float traceDistance,
                   FDebugDraw* debugDraw = nullptr) const;

    /// Unreal-like ACharacter::GetMesh() — skeletal visual + UAnimInstance.
    [[nodiscard]] SkeletalMeshComponent& GetMesh() { return mesh_; }
    [[nodiscard]] const SkeletalMeshComponent& GetMesh() const { return mesh_; }

    /// Apply replicated movement state (client proxy / snapshot).
    void ApplyReplicatedState(const glm::vec3& location, float yawDegrees, float velocityY,
                              bool grounded);

    /// Normalized locomotion blend input [0,1] for Mesh UAnimInstance UBlendSpace1D.
    void SetAnimBlendInput(float speedAlpha);
    [[nodiscard]] float GetAnimBlendInput() const { return animBlendInput_; }

    /// When true (default), yaw follows wish movement. When false, call FaceRotation / SetActorYaw.
    bool bOrientRotationToMovement = true;

    /// Unreal-like health (ACharacter lite).
    [[nodiscard]] float GetHealth() const { return health_; }
    [[nodiscard]] float GetMaxHealth() const { return maxHealth_; }
    void SetHealth(float health);
    void SetMaxHealth(float maxHealth);
    /// Returns applied damage; calls Die when health hits 0.
    virtual float TakeDamage(float DamageAmount);
    virtual void Die();
    void Revive(float NewHealth);
    [[nodiscard]] bool IsAlive() const { return bAlive_; }

    void Reset(const glm::vec3& location, float yawDegrees = 0.0f);
    void AddMovementInput(const glm::vec3& wishDirXZ);
    void Jump();

    /// Smoothly face a world yaw when bOrientRotationToMovement is false (packs may snap via
    /// SetActorYaw).
    void FaceRotation(float yawDegrees, float deltaTime);

    /// Move capsule against an explicit FPhysScene (unit tests / tools). Packs may override.
    virtual void PerformMovement(FPhysScene& physScene, float deltaTime,
                                 FDebugDraw* debugDraw = nullptr);
    /// Move against `GetWorld()->GetPhysicsScene()` (no-op if not in a World).
    void TickCharacterMovement(float deltaTime, FDebugDraw* debugDraw = nullptr);
    /// After FPhysScene::Step, push the capsule out of overlapping bodies.
    void ResolveOverlaps(FPhysScene& physScene);
    void ResolveOverlaps();

    /// Separate this capsule from another Character on XZ (equal share). No-op if Y ranges miss.
    void ResolvePawnOverlap(Character& other);

    /// Ticks Mesh UAnimInstance (Unreal: Character::Tick → Mesh component).
    void Tick(float deltaTime) override;

    /// Draw GetMesh() via SceneComponent world transform.
    void SubmitMeshDraw(FSceneRenderer& renderer) const;

private:
    void applyYaw(float targetYawDegrees, float deltaTime);
    void moveHorizontal(FPhysScene& physScene, float deltaTime, FDebugDraw* debugDraw);
    void integrateVertical(FPhysScene& physScene, float deltaTime, FDebugDraw* debugDraw);
    void resolveSides(FPhysScene& physScene, bool applyPush);

    /// Capsule cylinder half-height (excl. hemispherical caps) for CapsuleTrace.
    [[nodiscard]] float capsuleHalfHeight() const;
    [[nodiscard]] glm::vec3 capsuleCenterFromFeet(const glm::vec3& feet) const;
    /// True if a horizontal sweep should stop on this hit (not walkable floor/top).
    [[nodiscard]] bool blocksHorizontalMove(const FHitResult& hit) const;
    /// Unreal-like SafeMoveUpdatedComponent (XZ): sweep capsule, advance to hit, optional outHit.
    /// Returns true if the full delta was applied (no blocking side hit).
    bool safeMoveUpdatedComponent(FPhysScene& physScene, const glm::vec3& delta, FHitResult* outHit,
                                  FDebugDraw* debugDraw);
    /// Project velocity onto the wall plane (Unreal ComputeSlideVector lite, Y forced 0).
    [[nodiscard]] static glm::vec3 computeSlideVector(const glm::vec3& delta,
                                                      const glm::vec3& impactNormal);
    /// Unreal CMC step-up: raise ≤ MaxStepHeight, move forward, land on walkable floor.
    [[nodiscard]] bool tryStepUp(FPhysScene& physScene, const glm::vec3& forwardDelta,
                                 FDebugDraw* debugDraw);

    FCapsuleShape capsule_{};
    CharacterMovement movement_{};
    SkeletalMeshComponent mesh_{};
    float animBlendInput_ = 0.0f;
    float health_ = 100.0f;
    float maxHealth_ = 100.0f;
    bool bAlive_ = true;

    glm::vec3 wishDir_{0.0f};
    float velocityY_ = 0.0f;
    EMovementMode movementMode_ = EMovementMode::Walking;
    bool jumpRequested_ = false;
    bool justLanded_ = false;
    bool yawInitialized_ = false;
    int jumpsRemaining_ = 0;
    FindFloorResult currentFloor_{};
};

