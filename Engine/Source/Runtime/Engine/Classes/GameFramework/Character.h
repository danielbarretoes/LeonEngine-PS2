#pragma once

#include "CollisionQuery.h"
#include "CollisionShape.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Physics/PhysScene.h"

#include <glm/vec3.hpp>

#include <cstdint>

class FSceneRenderer;
class FDebugDraw;

/// Unreal-like EMovementMode (CMC lite: Walking / Falling only).
enum class EMovementMode : std::uint8_t
{
	None = 0,
	Walking,
	Falling,
};

/// Unreal-like FFindFloorResult (CMC floor query).
struct ENGINE_API FFindFloorResult
{
	bool bBlockingHit = false;
	bool bWalkableFloor = false;
	/// Distance from capsule feet down to floor ImpactPoint.y (>= 0 when hit below/at feet).
	float FloorDist = 0.0f;
	FHitResult Hit{};
};

/// Unreal-like UCharacterMovementComponent tunables (PascalCase Unreal-like field names).
struct ENGINE_API UCharacterMovementComponent
{
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
class ENGINE_API ACharacter : public APawn
{
public:
	ACharacter();

	void SetCapsule(const FCapsuleShape& InCapsule)
	{
		Capsule = InCapsule;
	}
	void SetCharacterMovement(const UCharacterMovementComponent& InMovement)
	{
		Movement = InMovement;
	}

	[[nodiscard]] const FCapsuleShape& GetCapsule() const
	{
		return Capsule;
	}
	[[nodiscard]] UCharacterMovementComponent& GetCharacterMovement()
	{
		return Movement;
	}
	[[nodiscard]] const UCharacterMovementComponent& GetCharacterMovement() const
	{
		return Movement;
	}

	/// Unreal-like UCharacterMovementComponent::SetMovementMode / MovementMode.
	void SetMovementMode(EMovementMode NewMode);
	[[nodiscard]] EMovementMode GetMovementMode() const
	{
		return MovementMode;
	}

	/// Unreal-like UCharacterMovementComponent::IsMovingOnGround.
	[[nodiscard]] bool IsMovingOnGround() const
	{
		return MovementMode == EMovementMode::Walking;
	}
	/// Unreal-like UCharacterMovementComponent::IsFalling (airborne / jumping).
	[[nodiscard]] bool IsFalling() const
	{
		return MovementMode == EMovementMode::Falling;
	}
	/// Vertical velocity (Unreal Velocity.Z) for jump SM apex detection.
	[[nodiscard]] float GetVelocityZ() const
	{
		return VelocityY;
	}
	/// True for one frame after leaving air → ground (consumed by UAnimInstance).
	[[nodiscard]] bool ConsumeJustLanded();

	/// Last successful FindFloor from integrateVertical (may be empty if never queried).
	[[nodiscard]] const FFindFloorResult& GetCurrentFloor() const
	{
		return CurrentFloor;
	}

	/// Unreal IsWalkable: ImpactNormal.Z >= WalkableFloorZ.
	[[nodiscard]] bool IsWalkable(const FHitResult& Hit) const;

	/// Unreal-like FindFloor: downward sphere trace from feet; fills outFloor.
	void FindFloor(
		FPhysScene& PhysScene, FFindFloorResult& OutFloor, float TraceDistance, FDebugDraw* DebugDraw = nullptr) const;

	/// Unreal-like ACharacter::GetMesh() — skeletal visual + UAnimInstance.
	[[nodiscard]] USkeletalMeshComponent& GetMesh()
	{
		return Mesh;
	}
	[[nodiscard]] const USkeletalMeshComponent& GetMesh() const
	{
		return Mesh;
	}

	/// Apply replicated movement state (client proxy / snapshot).
	void ApplyReplicatedState(const glm::vec3& Location, float YawDegrees, float InVelocityY, bool bGrounded);

	/// Normalized locomotion blend input [0,1] for Mesh UAnimInstance UBlendSpace1D.
	void SetAnimBlendInput(float SpeedAlpha);
	[[nodiscard]] float GetAnimBlendInput() const
	{
		return AnimBlendInput;
	}

	/// When true (default), yaw follows wish movement. When false, call FaceRotation / SetActorYaw.
	bool bOrientRotationToMovement = true;

	/// Unreal-like health (ACharacter lite).
	[[nodiscard]] float GetHealth() const
	{
		return Health;
	}
	[[nodiscard]] float GetMaxHealth() const
	{
		return MaxHealth;
	}
	void SetHealth(float InHealth);
	void SetMaxHealth(float InMaxHealth);
	/// Returns applied damage; calls Die when health hits 0.
	virtual float TakeDamage(float DamageAmount);
	virtual void Die();
	void Revive(float NewHealth);
	[[nodiscard]] bool IsAlive() const
	{
		return bAlive;
	}

	void Reset(const glm::vec3& Location, float YawDegrees = 0.0f);
	void AddMovementInput(const glm::vec3& WishDirXz);
	void Jump();

	/// Smoothly face a world yaw when bOrientRotationToMovement is false (games may snap via
	/// SetActorYaw).
	void FaceRotation(float YawDegrees, float DeltaTime);

	/// Move capsule against an explicit FPhysScene (unit tests / tools). Games may override.
	virtual void PerformMovement(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw = nullptr);
	/// Move against `GetWorld()->GetPhysicsScene()` (no-op if not in a World).
	void TickCharacterMovement(float DeltaTime, FDebugDraw* DebugDraw = nullptr);
	/// After FPhysScene::Step, push the capsule out of overlapping bodies.
	void ResolveOverlaps(FPhysScene& PhysScene);
	void ResolveOverlaps();

	/// Separate this capsule from another Character on XZ (equal share). No-op if Y ranges miss.
	void ResolvePawnOverlap(ACharacter& Other);

	/// Ticks Mesh UAnimInstance (Unreal: Character::Tick → Mesh component).
	void Tick(float DeltaTime) override;

	/// Draw GetMesh() via USceneComponent world transform.
	void SubmitMeshDraw(FSceneRenderer& Renderer) const;

private:
	void ApplyYaw(float TargetYawDegrees, float DeltaTime);
	void MoveHorizontal(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw);
	void IntegrateVertical(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw);
	void ResolveSides(FPhysScene& PhysScene, bool bApplyPush);

	/// Capsule cylinder half-height (excl. hemispherical caps) for CapsuleTrace.
	[[nodiscard]] float CapsuleHalfHeight() const;
	[[nodiscard]] glm::vec3 CapsuleCenterFromFeet(const glm::vec3& Feet) const;
	/// True if a horizontal sweep should stop on this hit (not walkable floor/top).
	[[nodiscard]] bool BlocksHorizontalMove(const FHitResult& Hit) const;
	/// Unreal-like SafeMoveUpdatedComponent (XZ): sweep capsule, advance to hit, optional outHit.
	/// Returns true if the full delta was applied (no blocking side hit).
	bool SafeMoveUpdatedComponent(
		FPhysScene& PhysScene, const glm::vec3& Delta, FHitResult* OutHit, FDebugDraw* DebugDraw);
	/// Project velocity onto the wall plane (Unreal ComputeSlideVector lite, Y forced 0).
	[[nodiscard]] static glm::vec3 ComputeSlideVector(const glm::vec3& Delta, const glm::vec3& ImpactNormal);
	/// Unreal CMC step-up: raise ≤ MaxStepHeight, move forward, land on walkable floor.
	[[nodiscard]] bool TryStepUp(FPhysScene& PhysScene, const glm::vec3& ForwardDelta, FDebugDraw* DebugDraw);

	FCapsuleShape Capsule{};
	UCharacterMovementComponent Movement{};
	USkeletalMeshComponent Mesh{};
	float AnimBlendInput = 0.0f;
	float Health = 100.0f;
	float MaxHealth = 100.0f;
	bool bAlive = true;

	glm::vec3 WishDir{0.0f};
	float VelocityY = 0.0f;
	EMovementMode MovementMode = EMovementMode::Walking;
	bool bJumpRequested = false;
	bool bJustLanded = false;
	bool bYawInitialized = false;
	int JumpsRemaining = 0;
	FFindFloorResult CurrentFloor{};
};
