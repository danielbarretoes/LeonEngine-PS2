#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShooterCharacterMovement.generated.h"

class ATriggerVolume;

/** ShooterGame's own movement modes (EMovementMode::Custom's ACharacter::GetCustomMovementMode; UE: a game's enum). */
enum class EShooterCustomMovementMode : uint8
{
	None = 0,
	/** On a ladder (UShooterCharacterMovement::PhysCustom). */
	Ladder = 1,
};

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
 * | PLAYER_MAX_SAFE_FALL_SPEED | 580 u/s (a 5.3 m drop) | SafeFallSpeed 1473 cm/s |
 * | PLAYER_FATAL_FALL_SPEED | 1024 u/s (100 before the multiplayer scale) | FatalFallSpeed 2601 cm/s |
 * | FlPlayerFallDamage's scale | 1.25 (lethal at 935 u/s, a 13.9 m drop) | FallDamageScale 1.25 |
 * | MAX_CLIMB_SPEED | 200 u/s | LadderClimbSpeed 508 cm/s |
 * | a jump off a ladder | 270 u/s | LadderJumpOffSpeed 686 cm/s |
 * | jump stamina (fuser2) | 1315.79 ms, 19 % a second | JumpStaminaTime 1.3158 s, JumpStaminaSlowdown 0.19 |
 * | m_flVelocityModifier | 0.5 on a hit | TaggingVelocityModifier 0.5, back in TaggingRecoveryTime 1 s |
 *
 * CS 1.6's movement on top of UE's model (ps2-shipping N30c):
 * - Fall damage (GetFallDamage; AShooterCharacter::Landed takes it): a landing faster than SafeFallSpeed takes
 *   (speed - SafeFallSpeed) x 100 / (FatalFallSpeed - SafeFallSpeed) x FallDamageScale health (CS 1.6's
 *   multiplayer FlPlayerFallDamage: DAMAGE_FOR_FALL_SPEED times 1.25), so all 100 at about 935 u/s (2375 cm/s).
 * - Jump stamina (CS's fuser2): a jump sets JumpStamina to JumpStaminaTime, which runs down with the time. On the
 *   floor, while it lasts, every CS command (10 ms) scales the horizontal velocity by GetJumpStaminaRatio, 1 -
 *   JumpStamina x JumpStaminaSlowdown (CS: (100 - fuser2 x 0.001 x 19) / 100): 0.75 at the jump, 0.87 at a
 *   normal jump's landing 0.67 s later. A step of DeltaTime scales it by the ratio to the power DeltaTime / 10 ms, the
 *   same slowdown at any step. A running jump's landing loses about 40 % of the speed, which comes back in half a
 *   second; the stamina runs out 1.3 s after the jump. No bunny hopping.
 * - Tagging (CS's m_flVelocityModifier): a shot that hurts the character (ApplyTagging) halves its horizontal
 *   velocity and its top speed (GetMaxSpeed), which come back linearly over TaggingRecoveryTime. The world's damage
 *   (a fall) does not tag.
 * - Ladders: a trigger volume tagged Ladder (AShooterGameMode::LadderTag; a map's Ladder node) is a ladder: a thin
 *   box against its wall (CS's func_ladder), whose face looks along the box's thinner horizontal axis, toward the side
 *   the climber is on. While the capsule touches one, the character is in the Ladder mode
 *   (EMovementMode::Custom): no gravity, and the input moves it as CS's PM_LadderMove does. The forward input
 *   (LadderClimbSpeed) goes along the view, its pitch included, the side input along the view's right; the part of
 *   that going into the ladder's face turns into climbing. So looking at the ladder climbs up at LadderClimbSpeed,
 *   looking straight down climbs down, and backing off it on the floor steps away; no input holds the character
 *   where it is. Jumping (JumpOffLadder) pushes it off at LadderJumpOffSpeed, and it falls; it grabs a ladder again
 *   only once it touches none. Leaving the volume (over the top, below, to a side) ends the mode with the climb's
 *   velocity, so climbing on carries the character up onto the ledge. The bots climb as the players do: their
 *   controller turns them to the ladder (GetLadderNormal) and looks up or down (ps2-polish P3).
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

	/** The fastest landing that does not hurt, cm/s (CS: PLAYER_MAX_SAFE_FALL_SPEED, 580 u/s). */
	UPROPERTY()
	float SafeFallSpeed = 1473.2f;

	/** The landing whose base damage is 100, cm/s (CS: PLAYER_FATAL_FALL_SPEED, 1024 u/s). */
	UPROPERTY()
	float FatalFallSpeed = 2601.0f;

	/**
	 * The multiplayer rules' scale on the fall damage (CS 1.6's FlPlayerFallDamage: 1.25), so a landing at about 935
	 * u/s (2375 cm/s, a 13.9 m drop) is already lethal.
	 */
	UPROPERTY()
	float FallDamageScale = 1.25f;

	/** How fast the forward input climbs a ladder, cm/s (CS: MAX_CLIMB_SPEED, 200 u/s). */
	UPROPERTY()
	float LadderClimbSpeed = 508.0f;

	/** How fast a jump pushes the climber off its ladder, cm/s (CS: 270 u/s). */
	UPROPERTY()
	float LadderJumpOffSpeed = 686.0f;

	/** Seconds of stamina a jump costs (CS: fuser2 = 1315.789 ms). */
	UPROPERTY()
	float JumpStaminaTime = 1.3158f;

	/** How much a second of stamina slows the character on the floor, per CS command (CS: 19 % a second). */
	UPROPERTY()
	float JumpStaminaSlowdown = 0.19f;

	/** The speed factor a hit leaves the character with (CS: m_flVelocityModifier 0.5). */
	UPROPERTY()
	float TaggingVelocityModifier = 0.5f;

	/** Seconds the speed takes to come back from a hit. */
	UPROPERTY()
	float TaggingRecoveryTime = 1.0f;

	/**
	 * The drawn weapon's speed modifier (AShooterWeapon::GetSpeedModifier), and walking on the ground the walk modifier
	 * on the running speed, not on the crouched one (UE ShooterGame: the targeting and running modifiers); times the
	 * tagging's GetVelocityModifier.
	 */
	[[nodiscard]] float GetMaxSpeed() const override;

	/**
	 * The damage of a landing at LandingSpeed cm/s (downward, positive): 0 up to SafeFallSpeed, then (LandingSpeed -
	 * SafeFallSpeed) x 100 / (FatalFallSpeed - SafeFallSpeed) x FallDamageScale (125 at FatalFallSpeed).
	 */
	[[nodiscard]] float GetFallDamage(float LandingSpeed) const;

	/** A jump left the floor: the stamina starts over (AShooterCharacter::OnJumped). */
	void StartJumpStamina();
	/** Seconds of jump stamina left. */
	[[nodiscard]] float GetJumpStamina() const
	{
		return JumpStamina;
	}
	/** The horizontal velocity's scale per CS command (10 ms) with Stamina seconds left: 1 - Stamina x Slowdown. */
	[[nodiscard]] float GetJumpStaminaRatio(float Stamina) const;

	/** A shot hurt the character: the velocity and the speed drop to TaggingVelocityModifier (the class comment). */
	void ApplyTagging();
	/** The tagging's speed factor, TaggingVelocityModifier to 1. */
	[[nodiscard]] float GetVelocityModifier() const
	{
		return VelocityModifier;
	}

	/** The stamina and the tagging run down by DeltaTime (CS: PM_ReduceTimers; AShooterCharacter's move calls it). */
	void UpdateMovementTimers(float DeltaTime);
	/** No stamina, no tagging, off any ladder (a new round). */
	void ResetMovementModifiers();

	/** The character is in the Ladder mode. */
	[[nodiscard]] bool IsOnLadder() const;
	/** The ladder the character is on, null off one. */
	[[nodiscard]] ATriggerVolume* GetLadder() const
	{
		return CurrentLadder;
	}
	/** The face of the ladder the character is on (level, toward the character's side), zero off one. */
	[[nodiscard]] FVector GetLadderNormal() const;
	/** A jump on the ladder: the next move pushes the character off it (AShooterCharacter::Jump). */
	void JumpOffLadder();

	/** CS's PM_WalkMove: on the floor, the jump stamina slows the horizontal velocity first. */
	void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	/** Crouching, then the ladder: grabbed when touched, let go when left. */
	void UpdateCharacterStateBeforeMovement(FPhysScene& PhysScene) override;
	/** The Ladder mode's move (the class comment). */
	void PhysCustom(FPhysScene& PhysScene, float DeltaTime) override;

private:
	/** The ladder the capsule touches (the game mode's ladder volumes), or null. */
	[[nodiscard]] ATriggerVolume* FindTouchedLadder() const;
	/** CS's PM_LadderMove. */
	void PhysLadder(FPhysScene& PhysScene, float DeltaTime);

	/** The ladder the character climbs. */
	UPROPERTY(Transient)
	ATriggerVolume* CurrentLadder = nullptr;

	/** Seconds of jump stamina left (CS: fuser2). */
	float JumpStamina = 0.0f;
	/** The tagging's speed factor (CS: m_flVelocityModifier). */
	float VelocityModifier = 1.0f;
	/** A jump off the ladder waits for the next move. */
	bool bWantsToJumpOffLadder = false;
	/** Jumped off a ladder and still touching one: not grabbed again until it touches none. */
	bool bJumpedOffLadder = false;
};
