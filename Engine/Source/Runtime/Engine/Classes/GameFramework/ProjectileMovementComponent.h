#pragma once

#include "CollisionQuery.h"
#include "CoreMinimal.h"
#include "GameFramework/MovementComponent.h"
#include "ProjectileMovementComponent.generated.h"

/** A projectile bounced (UE: FOnProjectileBounceDelegate): the hit and the velocity before the bounce. */
using FOnProjectileBounceDelegate =
	TMulticastDelegate<void(const FHitResult& /*ImpactResult*/, const FVector& /*ImpactVelocity*/)>;
/** A projectile stopped (UE: FOnProjectileStopDelegate): the hit that stopped it. */
using FOnProjectileStopDelegate = TMulticastDelegate<void(const FHitResult& /*ImpactResult*/)>;

/**
 * Moves its updated component as a projectile: a velocity, gravity, and on a blocking hit a bounce or a stop (UE:
 * UProjectileMovementComponent).
 *
 * - At spawn (InitializeComponent) Velocity's direction takes InitialSpeed, in the updated component's space when
 *   bInitialVelocityInLocalSpace (UE's defaults: Velocity (1, 0, 0), local), so a projectile spawned facing its shot
 *   flies along it.
 * - Each tick, in steps of at most MaxSimulationTimeStep: gravity (the world's GetGravityZ times
 *   ProjectileGravityScale) accelerates the velocity (limited to MaxSpeed), and the updated component is swept along
 *   the move with its collision shape, on its object type with its responses, ignoring its owner and MoveIgnoreActors
 *   (UE's MoveUpdatedComponent with a sweep). A component that is not a primitive moves without a sweep.
 * - A blocking hit ends the step there. With bShouldBounce the velocity reflects (ComputeBounceResult: Bounciness of
 *   the normal part, Friction off the tangential part), OnProjectileBounce is broadcast, and the rest of the step goes
 *   on; below BounceVelocityStopSimulatingThreshold, or without bShouldBounce, the projectile stops: the velocity is
 *   zeroed, the updated component let go and OnProjectileStop broadcast (UE's StopSimulating).
 *
 * Leon: no homing, no sliding along a surface (a resting projectile bounces to a stop), no interpolated visual
 * component.
 */
UCLASS()
class ENGINE_API UProjectileMovementComponent : public UMovementComponent
{
	GENERATED_BODY()

public:
	UProjectileMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The speed at spawn, cm/s; 0 keeps Velocity's length (UE: InitialSpeed). */
	UPROPERTY()
	float InitialSpeed = 0.0f;

	/** The speed limit, cm/s; 0 is none (UE: MaxSpeed). */
	UPROPERTY()
	float MaxSpeed = 0.0f;

	/** The rotation follows the velocity each step (UE: bRotationFollowsVelocity). */
	UPROPERTY()
	uint8 bRotationFollowsVelocity : 1;

	/** A blocking hit bounces instead of stopping (UE: bShouldBounce). */
	UPROPERTY()
	uint8 bShouldBounce : 1;

	/** Velocity at spawn is in the updated component's space (UE: bInitialVelocityInLocalSpace). */
	UPROPERTY()
	uint8 bInitialVelocityInLocalSpace : 1;

	/** The simulation runs (UE: bSimulationEnabled). */
	UPROPERTY()
	uint8 bSimulationEnabled : 1;

	/** The world's gravity times this; 0 flies straight (UE: ProjectileGravityScale). */
	UPROPERTY()
	float ProjectileGravityScale = 1.0f;

	/** The part of the normal speed a bounce keeps (UE: Bounciness, the coefficient of restitution). */
	UPROPERTY()
	float Bounciness = 0.6f;

	/** The part of the tangential speed a bounce takes away (UE: Friction). */
	UPROPERTY()
	float Friction = 0.2f;

	/** A bounce slower than this stops the projectile, cm/s (UE: BounceVelocityStopSimulatingThreshold). */
	UPROPERTY()
	float BounceVelocityStopSimulatingThreshold = 5.0f;

	/** The longest step of the simulation, seconds (UE: MaxSimulationTimeStep). */
	UPROPERTY()
	float MaxSimulationTimeStep = 0.05f;

	/** The most steps (and bounces) in one tick (UE: MaxSimulationIterations). */
	UPROPERTY()
	int32 MaxSimulationIterations = 8;

	/** Called on each bounce (UE: OnProjectileBounce). */
	FOnProjectileBounceDelegate OnProjectileBounce;
	/** Called when the projectile stops (UE: OnProjectileStop). */
	FOnProjectileStopDelegate OnProjectileStop;

	/** Sets Velocity from a vector in the updated component's space (UE: SetVelocityInLocalSpace). */
	void SetVelocityInLocalSpace(const FVector& NewVelocity);

	/** Stops: zero velocity, no updated component, OnProjectileStop (UE: StopSimulating). */
	void StopSimulating(const FHitResult& HitResult);
	/** True once stopped: no updated component or the simulation disabled (UE: HasStoppedSimulation). */
	[[nodiscard]] bool HasStoppedSimulation() const
	{
		return UpdatedComponent == nullptr || !bSimulationEnabled;
	}

	/** The acceleration along Z: the world's gravity times ProjectileGravityScale, cm/s^2 (UE: GetGravityZ). */
	[[nodiscard]] float GetGravityZ() const;
	/** The velocity after DeltaTime of gravity, limited to MaxSpeed (UE: ComputeVelocity). */
	[[nodiscard]] FVector ComputeVelocity(const FVector& InitialVelocity, float DeltaTime) const;
	/** The move in DeltaTime from InVelocity under gravity (UE: ComputeMoveDelta). */
	[[nodiscard]] FVector ComputeMoveDelta(const FVector& InVelocity, float DeltaTime) const;
	/** The velocity after bouncing off Hit (UE: ComputeBounceResult). */
	[[nodiscard]] FVector ComputeBounceResult(const FHitResult& Hit) const;
	/** The velocity clamped to MaxSpeed (UE: LimitVelocity). */
	[[nodiscard]] FVector LimitVelocity(FVector NewVelocity) const;

	[[nodiscard]] float GetMaxSpeed() const override
	{
		return MaxSpeed;
	}

	/** Applies InitialSpeed and the local space to Velocity (UE). */
	void InitializeComponent() override;
	/** Runs the simulation (UE: TickComponent). */
	void TickComponent(float DeltaTime) override;

private:
	/**
	 * Moves the updated component by Delta, sweeping a primitive's shape; OutHit is the blocking hit, if any (UE:
	 * MoveUpdatedComponent with bSweep).
	 */
	void MoveUpdatedComponent(const FVector& Delta, FHitResult& OutHit);
	/** A blocking hit: bounce or stop; true when the simulation goes on (UE: HandleImpact). */
	bool HandleImpact(const FHitResult& Hit);
};
