#pragma once

#include "CollisionQuery.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Physics/PhysScene.h"
#include "Character.generated.h"

class FDebugDraw;

/**
 * Kinematic capsule pawn (Unreal-style ACharacter + CMC lite).
 *
 * Components (default subobjects, UE names): the root UCapsuleComponent "CollisionCylinder", the
 * UCharacterMovementComponent "CharMoveComp" (its tunables) and the USkeletalMeshComponent "CharacterMesh0" attached to
 * the capsule.
 *
 * Contract:
 * - Actor location = capsule **feet** (bottom), not capsule center (UE: the capsule center; a documented deviation).
 * - The capsule extends up (+Z) by twice its half height from the feet; XY radius = capsule radius.
 * - Actor yaw is a UE yaw (0 faces +X, 90 faces +Y); the mesh shows legacy content with a relative yaw of
 *   LegacyContentYaw.
 * - The capsule is a query-only body of the physics scene (P17): a Pawn object that ignores the Visibility channel
 *   (UE's Pawn profile) and the Pawn channel (Leon: the world separates pawns, ResolvePawnOverlap), so traces on the
 *   other channels hit characters. The movement's queries run on the capsule's object type with its responses and
 *   ignore the capsule itself; after moving, the character sends its capsule's body the new place.
 * - Moves via PerformMovement queries.
 * - Modes: Walking / Falling via SetMovementMode; floor via FindFloor → IsWalkable.
 * - Horizontal speed: the movement component's instant model by default, or UE's velocity model (acceleration,
 *   friction, braking, air control; UCharacterMovementComponent::bInstantVelocity). Crouching (Crouch / UnCrouch,
 *   NavAgentProps.bCanCrouch) shrinks the capsule to CrouchedHalfHeight before the next move.
 * - Input: the legacy AddMovementInput(Wish) keeps a wish direction until changed; the pawn's input vector
 *   (APawn::AddMovementInput, the player's axes) is consumed each world tick, as UE's movement does.
 */
UCLASS()
class ENGINE_API ACharacter : public APawn
{
	GENERATED_BODY()

public:
	ACharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Names of the default subobjects (UE). */
	static const FName CapsuleComponentName;
	static const FName CharacterMovementComponentName;
	static const FName MeshComponentName;

	/** Resizes the capsule component from a capsule shape (radius, half height). */
	void SetCapsule(const FCollisionShape& InCapsule)
	{
		CapsuleComponent->SetCapsuleSize(InCapsule.GetCapsuleRadius(), InCapsule.GetCapsuleHalfHeight());
	}

	/** The capsule component's shape (radius 35 cm, half height 92.5 cm by default). */
	[[nodiscard]] FCollisionShape GetCapsule() const
	{
		return CapsuleComponent->GetCollisionShape();
	}
	/** The root capsule (UE: GetCapsuleComponent). */
	[[nodiscard]] UCapsuleComponent* GetCapsuleComponent() const
	{
		return CapsuleComponent;
	}
	/**
	 * The movement component (UE: GetCharacterMovement). A reference (UE returns the pointer): every character has its
	 * movement default subobject.
	 */
	[[nodiscard]] UCharacterMovementComponent& GetCharacterMovement()
	{
		return *CharacterMovement;
	}
	[[nodiscard]] const UCharacterMovementComponent& GetCharacterMovement() const
	{
		return *CharacterMovement;
	}

	/** Unreal-like UCharacterMovementComponent::SetMovementMode / MovementMode. */
	void SetMovementMode(EMovementMode NewMode);
	[[nodiscard]] EMovementMode GetMovementMode() const
	{
		return MovementMode;
	}

	/** Unreal-like UCharacterMovementComponent::IsMovingOnGround. */
	[[nodiscard]] bool IsMovingOnGround() const
	{
		return MovementMode == EMovementMode::Walking;
	}
	/** Unreal-like UCharacterMovementComponent::IsFalling (airborne / jumping). */
	[[nodiscard]] bool IsFalling() const
	{
		return MovementMode == EMovementMode::Falling;
	}
	/** Vertical velocity (Unreal Velocity.Z) for jump SM apex detection. */
	[[nodiscard]] float GetVelocityZ() const
	{
		return VelocityZ;
	}
	/** True for one frame after leaving air → ground (consumed by UAnimInstance). */
	[[nodiscard]] bool ConsumeJustLanded();

	/** Last successful FindFloor from integrateVertical (may be empty if never queried). */
	[[nodiscard]] const FFindFloorResult& GetCurrentFloor() const
	{
		return CurrentFloor;
	}

	/** Unreal IsWalkable: ImpactNormal.Z >= WalkableFloorZ. */
	[[nodiscard]] bool IsWalkable(const FHitResult& Hit) const;

	/** Unreal-like FindFloor: downward sphere trace from feet; fills outFloor. */
	void FindFloor(
		FPhysScene& PhysScene, FFindFloorResult& OutFloor, float TraceDistance, FDebugDraw* DebugDraw = nullptr) const;

	/**
	 * Unreal-like ACharacter::GetMesh() — skeletal visual + UAnimInstance. A reference (UE returns the pointer): every
	 * character has its mesh default subobject.
	 */
	[[nodiscard]] USkeletalMeshComponent& GetMesh()
	{
		return *Mesh;
	}
	[[nodiscard]] const USkeletalMeshComponent& GetMesh() const
	{
		return *Mesh;
	}

	/** Apply replicated movement state (client proxy / snapshot). */
	void ApplyReplicatedState(const FVector& Location, const FRotator& Rotation, float InVelocityZ, bool bGrounded);

	/** Normalized locomotion blend input [0,1] for Mesh UAnimInstance UBlendSpace1D. */
	void SetAnimBlendInput(float SpeedAlpha);
	[[nodiscard]] float GetAnimBlendInput() const
	{
		return AnimBlendInput;
	}

	/** When true (default), yaw follows wish movement. When false, call FaceRotation / SetActorRotation. */
	UPROPERTY()
	bool bOrientRotationToMovement = true;

	void Reset(const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);
	void AddMovementInput(const FVector& WishDirXY);
	void Jump();

	/**
	 * Smoothly turn to the yaw of NewRotation when bOrientRotationToMovement is false (UE: FaceRotation; games may
	 * snap via SetActorRotation).
	 */
	void FaceRotation(const FRotator& NewRotation, float DeltaTime);

	/** Move capsule against an explicit FPhysScene (unit tests / tools). Games may override. */
	virtual void PerformMovement(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw = nullptr);
	/** Move against GetWorld()->GetPhysicsScene() (no-op if not in a World). */
	void TickCharacterMovement(float DeltaTime, FDebugDraw* DebugDraw = nullptr);
	/** After FPhysScene::Step, push the capsule out of overlapping bodies. */
	void ResolveOverlaps(FPhysScene& PhysScene);
	void ResolveOverlaps();

	/** Separate this capsule from another Character on XY (equal share). No-op if the Z ranges miss. */
	void ResolvePawnOverlap(ACharacter& Other);

	/** Asks to crouch before the next move when the character can (UE: Crouch, CanCrouch). */
	virtual void Crouch(bool bClientSimulation = false);
	/** Asks to stand up before the next move, when there is room (UE: UnCrouch). */
	virtual void UnCrouch(bool bClientSimulation = false);
	/** The character may crouch: its movement can ever crouch and it is not crouched yet (UE: CanCrouch). */
	[[nodiscard]] bool CanCrouch() const;
	/**
	 * The capsule shrank (UE: OnStartCrouch): HalfHeightAdjust is how much its half height lost. The eyes come down
	 * (RecalculateBaseEyeHeight); UE also raises the mesh, which Leon's feet-based capsule does not need.
	 */
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);
	/** The capsule grew back (UE: OnEndCrouch). */
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);
	/** The eyes: CrouchedEyeHeight while crouched, the class default otherwise (UE). */
	void RecalculateBaseEyeHeight() override;

	/** The character is crouched (UE: bIsCrouched; UCharacterMovementComponent::Crouch sets it). */
	UPROPERTY(Transient)
	uint8 bIsCrouched : 1;

	/** The eyes above the actor location while crouched, cm (UE: CrouchedEyeHeight). */
	UPROPERTY()
	float CrouchedEyeHeight = 40.0f;

	/**
	 * The query parameters of the movement's traces (UE: UPrimitiveComponent::InitSweepCollisionParams): the capsule
	 * ignored, the query's responses to the object types the capsule's own.
	 */
	void InitCollisionParams(FCollisionQueryParams& OutParams, FCollisionResponseParams& OutResponseParam) const;
	/** The channel the movement traces on: the capsule's object type (UE: UpdatedComponent's). */
	[[nodiscard]] ECollisionChannel GetMovementTraceChannel() const
	{
		return CapsuleComponent->GetCollisionObjectType();
	}

	/** Ticks Mesh UAnimInstance (Unreal: Character::Tick → Mesh component). */
	void Tick(float DeltaTime) override;

private:
	/** The movement component changes the capsule, the crouch state and the feet (UE's is a friend too). */
	friend class UCharacterMovementComponent;

	void ApplyYaw(float TargetYaw, float DeltaTime);
	void MoveHorizontal(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw);
	/** UE's velocity model (UCharacterMovementComponent::bInstantVelocity false): CalcVelocity, then the move. */
	void MoveHorizontalWithVelocity(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw);
	/** Sweeps Remaining along the floor with the step-up and the slides (both horizontal models). */
	void MoveAlongFloor(FPhysScene& PhysScene, FVector Remaining, float DeltaTime, FDebugDraw* DebugDraw);
	void IntegrateVertical(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw);
	void ResolveSides(FPhysScene& PhysScene, bool bApplyPush);

	/** Capsule cylinder half-height (excl. hemispherical caps) for CapsuleTrace. */
	[[nodiscard]] float CapsuleHalfHeight() const;
	[[nodiscard]] FVector CapsuleCenterFromFeet(const FVector& Feet) const;
	/** True if a horizontal sweep should stop on this hit (not walkable floor/top). */
	[[nodiscard]] bool BlocksHorizontalMove(const FHitResult& Hit) const;
	/**
	 * Unreal-like SafeMoveUpdatedComponent (XY): sweep capsule, advance to hit, optional outHit.
	 * Returns true if the full delta was applied (no blocking side hit).
	 */
	bool SafeMoveUpdatedComponent(
		FPhysScene& PhysScene, const FVector& Delta, FHitResult* OutHit, FDebugDraw* DebugDraw);
	/** Project velocity onto the wall plane (Unreal ComputeSlideVector lite, Z forced 0). */
	[[nodiscard]] static FVector ComputeSlideVector(const FVector& Delta, const FVector& ImpactNormal);
	/** Unreal CMC step-up: raise ≤ MaxStepHeight, move forward, land on walkable floor. */
	[[nodiscard]] bool TryStepUp(FPhysScene& PhysScene, const FVector& ForwardDelta, FDebugDraw* DebugDraw);

	/** The root: the character's collision capsule (UE: CapsuleComponent). */
	UPROPERTY()
	UCapsuleComponent* CapsuleComponent = nullptr;

	/** The movement tunables (UE: CharacterMovement). */
	UPROPERTY()
	UCharacterMovementComponent* CharacterMovement = nullptr;

	/** The skeletal visual, attached to the root (UE: Mesh). */
	UPROPERTY()
	USkeletalMeshComponent* Mesh = nullptr;

	float AnimBlendInput = 0.0f;

	FVector WishDir = FVector::ZeroVector;
	float VelocityZ = 0.0f;
	EMovementMode MovementMode = EMovementMode::Walking;
	bool bJumpRequested = false;
	/** WishDir came from the pawn's input vector, which clears it when the input stops. */
	bool bWishFromInputVector = false;
	bool bJustLanded = false;
	bool bYawInitialized = false;
	int JumpsRemaining = 0;
	FFindFloorResult CurrentFloor{};
};
