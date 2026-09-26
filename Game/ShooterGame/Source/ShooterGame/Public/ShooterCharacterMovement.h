#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShooterCharacterMovement.generated.h"

/**
 * The character movement of ShooterGame (UE ShooterGame: UShooterCharacterMovement, which changes GetMaxSpeed for
 * running and aiming): Counter-Strike's movement in UE's velocity model, and the walk key's speed.
 *
 * The values are Counter-Strike's (1.6 / Source) converted at their unit, 1 unit = 1 inch = 2.54 cm (the CS player is
 * 72 units tall, 183 cm; Leon's standing capsule is 183 cm):
 *
 * | CS | value | here |
 * | --- | --- | --- |
 * | run speed (knife / pistol) | 250 u/s | MaxWalkSpeed 635 cm/s |
 * | walk (shift) | 52 % | WalkSpeedModifier 0.52 (330 cm/s) |
 * | crouched speed | 1 / 3 | MaxWalkSpeedCrouched 212 cm/s |
 * | sv_accelerate 5 (x speed) | 1250 u/s^2 | MaxAcceleration 3175 cm/s^2 (full speed in 0.2 s) |
 * | sv_friction 4 | 4 / s | GroundFriction 4, BrakingFrictionFactor 1 |
 * | sv_stopspeed 75 x friction | 300 u/s^2 | BrakingDecelerationWalking 762 cm/s^2 |
 * | sv_gravity 800 | 800 u/s^2 | Gravity 2032 cm/s^2 |
 * | jump impulse sqrt(2 x 800 x 45) | 268 u/s | JumpZVelocity 682 cm/s (a 114 cm jump) |
 * | step size 18 | 18 u | MaxStepHeight 45 cm |
 * | walkable slope 0.7 | 0.7 | WalkableFloorZ 0.7 |
 * | air control | (air accelerate 10, 30 u/s cap) | AirControl 0.3 of MaxAcceleration |
 */
UCLASS(Config = Game)
class SHOOTERGAME_API UShooterCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UShooterCharacterMovement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The walk key's fraction of the speed (CS: 52 %), from [/Script/ShooterGame.ShooterCharacterMovement]. */
	UPROPERTY(Config)
	float WalkSpeedModifier = 0.52f;

	/** The walk key is held (AShooterCharacter::SetWalking). */
	UPROPERTY(Transient)
	bool bIsWalking = false;

	/**
	 * The drawn weapon's speed modifier (AShooterWeapon::GetSpeedModifier), and walking on the ground the walk modifier
	 * on the running speed, not on the crouched one (UE ShooterGame: the targeting and running modifiers).
	 */
	[[nodiscard]] float GetMaxSpeed() const override;
};
