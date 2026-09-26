#pragma once

#include "CollisionQuery.h"
#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "CharacterMovementComponent.generated.h"

class ACharacter;
class FPhysScene;

/** Unreal-like EMovementMode (CMC lite: Walking / Falling only). */
enum class EMovementMode : uint8
{
	None = 0,
	Walking,
	Falling,
};

/** Unreal-like FFindFloorResult (CMC floor query). */
struct ENGINE_API FFindFloorResult
{
	bool bBlockingHit = false;
	bool bWalkableFloor = false;
	/** Distance from capsule feet down to floor ImpactPoint.Z (>= 0 when hit below/at feet). */
	float FloorDist = 0.0f;
	FHitResult Hit{};
};

/**
 * The movement of a character (UE: UCharacterMovementComponent): the tunables of Leon's kinematic capsule movement,
 * UE's velocity model (CalcVelocity, ApplyVelocityBraking) and crouching (Crouch, UnCrouch). ACharacter still runs the
 * movement itself (PerformMovement: walking, falling, step-up, slides) with these values; moving that code here is
 * future work (a documented P12 deviation).
 *
 * Two horizontal models:
 * - bInstantVelocity (Leon's default, the CMC lite the golden tables pin): the character moves at GetMaxSpeed along
 *   its input at once and keeps no horizontal velocity of its own; falling, it moves at AirControl times that speed.
 * - UE's (bInstantVelocity false, P17): the input is an acceleration of MaxAcceleration; on the ground CalcVelocity
 *   turns the velocity toward it with GroundFriction and, without input or above the speed, brakes with the
 *   friction and BrakingDecelerationWalking (ApplyVelocityBraking); in the air the velocity is kept and the input
 *   accelerates it by AirControl (FallingLateralFriction, BrakingDecelerationFalling).
 */
UCLASS()
class ENGINE_API UCharacterMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UCharacterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: MIN_TICK_TIME, the shortest step that moves the character (s). */
	static constexpr float MIN_TICK_TIME = 1.0e-6f;

	/** Unreal MaxWalkSpeed (cm/s). */
	UPROPERTY()
	float MaxWalkSpeed = 450.0f;

	/** Unreal MaxWalkSpeedCrouched (cm/s): the ground speed while crouched. */
	UPROPERTY()
	float MaxWalkSpeedCrouched = 300.0f;

	/** Unreal JumpZVelocity (cm/s). */
	UPROPERTY()
	float JumpZVelocity = 700.0f;

	/** World gravity acceleration, cm/s^2 (Leon absolute; UE uses GravityScale × world gravity). */
	UPROPERTY()
	float Gravity = 2400.0f;

	UPROPERTY()
	float TurnSharpness = 16.0f;

	/** Added to the yaw the character turns to when it orients to its movement (degrees). */
	UPROPERTY()
	float ModelYawOffset = 0.0f;

	/** Height of the infinite floor plane (cm). */
	UPROPERTY()
	float FloorZ = 0.0f;

	/** Contact skin (cm). */
	UPROPERTY()
	float Skin = 2.0f;

	/** Unreal MaxStepHeight (cm): geometric step-up + floor probe window. */
	UPROPERTY()
	float MaxStepHeight = 35.0f;

	/** Half size of the square the character may walk in (cm). */
	UPROPERTY()
	float WalkBounds = 1800.0f;

	/** Unitless push strength against dynamic bodies. */
	UPROPERTY()
	float PushStrength = 1.0f;

	UPROPERTY()
	float PushDamping = 6.0f;

	/** Unreal WalkableFloorZ (cos of max walkable slope). Default ~44° (UE). */
	UPROPERTY()
	float WalkableFloorZ = 0.71f;

	/**
	 * Unreal AirControl [0,1]. With bInstantVelocity, the fraction of the speed the input moves the character while
	 * Falling (Leon's lite); in UE's model, the fraction of MaxAcceleration the input accelerates it with (UE).
	 */
	UPROPERTY()
	float AirControl = 0.35f;

	/** Leon: the lite horizontal model (see the class comment). False runs UE's velocity model. */
	UPROPERTY()
	bool bInstantVelocity = true;

	/** Unreal MaxAcceleration (cm/s^2): what a full input accelerates by (UE's model). */
	UPROPERTY()
	float MaxAcceleration = 2048.0f;

	/**
	 * Unreal GroundFriction: how fast the velocity turns toward the input while walking and, times
	 * BrakingFrictionFactor, how fast it brakes (UE's model).
	 */
	UPROPERTY()
	float GroundFriction = 8.0f;

	/** Unreal BrakingDecelerationWalking (cm/s^2): the constant part of the braking on the ground (UE's model). */
	UPROPERTY()
	float BrakingDecelerationWalking = 2048.0f;

	/** Unreal BrakingDecelerationFalling (cm/s^2): the braking in the air, 0 keeps the velocity (UE's model). */
	UPROPERTY()
	float BrakingDecelerationFalling = 0.0f;

	/** Unreal BrakingFrictionFactor: multiplies the friction the braking uses (UE's model). */
	UPROPERTY()
	float BrakingFrictionFactor = 2.0f;

	/** Unreal BrakingFriction: the braking friction when bUseSeparateBrakingFriction (UE's model). */
	UPROPERTY()
	float BrakingFriction = 0.0f;

	/** Unreal bUseSeparateBrakingFriction: brake with BrakingFriction instead of the ground friction (UE's model). */
	UPROPERTY()
	bool bUseSeparateBrakingFriction = false;

	/** Unreal FallingLateralFriction: the friction of the air (UE's model). */
	UPROPERTY()
	float FallingLateralFriction = 0.0f;

	/** Unreal AirControlBoostMultiplier: multiplies AirControl below the threshold speed (UE's model). */
	UPROPERTY()
	float AirControlBoostMultiplier = 2.0f;

	/** Unreal AirControlBoostVelocityThreshold (cm/s): below it AirControl is boosted (UE's model). */
	UPROPERTY()
	float AirControlBoostVelocityThreshold = 25.0f;

	/** Max jumps from ground before landing (1 = normal, 2 = double jump). Projects may raise. */
	UPROPERTY()
	int32 MaxJumpCount = 1;

	/**
	 * Unreal CrouchedHalfHeight (cm): the capsule's half height while crouched. The character's capsule stands on its
	 * feet, so on the ground the feet stay and the top comes down (UE's bCrouchMaintainsBaseLocation); in the air the
	 * capsule shrinks around its centre as UE's does (the feet come up).
	 */
	UPROPERTY()
	float CrouchedHalfHeight = 40.0f;

	/** Unreal bWantsToCrouch: the character crouches (or stands up) before its next move (ACharacter::Crouch). */
	UPROPERTY(Transient)
	uint8 bWantsToCrouch : 1;

	/** The character that owns the component (UE: CharacterOwner). */
	[[nodiscard]] ACharacter* GetCharacterOwner() const
	{
		return CharacterOwner;
	}

	/**
	 * The speed of the current mode (UE: GetMaxSpeed): walking, MaxWalkSpeedCrouched while crouched, else MaxWalkSpeed;
	 * falling, MaxWalkSpeed. A game changes it by overriding this (UE: ShooterGame's running / targeting modifiers).
	 */
	[[nodiscard]] float GetMaxSpeed() const override;

	/** The acceleration the last move applied, cm/s^2 (UE: GetCurrentAcceleration; UE's model only). */
	[[nodiscard]] FVector GetCurrentAcceleration() const
	{
		return Acceleration;
	}

	/**
	 * UE's CalcVelocity: updates Velocity (horizontal) toward the current Acceleration, with Friction for the turn and,
	 * without an input or above the speed, ApplyVelocityBraking with BrakingDeceleration.
	 */
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration);

	/** UE's ApplyVelocityBraking: slows Velocity down by friction and a constant deceleration, in sub-steps. */
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration);

	/** UE's GetAirControl: the fall acceleration the input gives (AirControl, boosted at low speed). */
	[[nodiscard]] virtual FVector GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration);

	/** The character is crouched (UE: IsCrouching). */
	[[nodiscard]] bool IsCrouching() const;
	/** The character may crouch at all (UE: CanEverCrouch: NavAgentProps.bCanCrouch). */
	[[nodiscard]] bool CanEverCrouch() const
	{
		return NavAgentProps.bCanCrouch;
	}
	/** The character may crouch now (UE: CanCrouchInCurrentState): it can ever crouch and moves on the ground or in the
	 * air. */
	[[nodiscard]] virtual bool CanCrouchInCurrentState() const;

	/**
	 * Crouches (UE: Crouch): the capsule takes CrouchedHalfHeight, the character is crouched and OnStartCrouch runs.
	 * The feet stay on the ground; in the air they come up by the half height change.
	 */
	virtual void Crouch(bool bClientSimulation = false);
	/**
	 * Stands up (UE: UnCrouch) when there is room: the capsule of the character's class default must fit, else the
	 * character stays crouched and the call returns false (UE checks the room the same way, by a query).
	 */
	virtual bool UnCrouch(bool bClientSimulation = false);

	/**
	 * Crouches or stands up as bWantsToCrouch asks before a move (UE: UpdateCharacterStateBeforeMovement); the room
	 * to stand up is tested in PhysScene.
	 */
	virtual void UpdateCharacterStateBeforeMovement(FPhysScene& PhysScene);

	/**
	 * Moves the owning character in its world (UE: TickComponent runs PerformMovement): the component ticks with its
	 * character, after the character's controller processed its input.
	 */
	void TickComponent(float DeltaTime) override;

	void PostInitProperties() override;

protected:
	/** UE: CharacterOwner. */
	UPROPERTY(Transient)
	ACharacter* CharacterOwner = nullptr;

	/** The acceleration of the current move, cm/s^2 (UE: Acceleration). */
	FVector Acceleration = FVector::ZeroVector;

	/** The input's size in [0, 1], which scales the speed (UE: AnalogInputModifier). */
	float AnalogInputModifier = 0.0f;

	/** The scene UnCrouch tests the room in, during UpdateCharacterStateBeforeMovement (else the world's). */
	FPhysScene* CrouchPhysScene = nullptr;

private:
	friend class ACharacter;
};
