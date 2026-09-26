#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Weapons/ShooterWeapon.h"
#include "ShooterWeapon_Instant.generated.h"

/**
 * A hitscan weapon (UE ShooterGame: AShooterWeapon_Instant): each shot is a line on the Weapon channel from the
 * owner's eyes, within the current spread, out to WeaponRange; what it hits takes point damage
 * (UGameplayStatics::ApplyPointDamage, this weapon the causer).
 *
 * Counter-Strike's model, in degrees and centimetres (1 CS unit = 2.54 cm):
 * - Damage: HitDamage x RangeModifier^(distance / RangeModifierDistance) (CS: the range modifier per 500 units,
 *   1270 cm). The victim multiplies a head hit by HeadshotMultiplier and applies its armor with ArmorRatio
 *   (AShooterCharacter::TakeDamage).
 * - Spread, a cone's half angle: WeaponSpread, plus MovingSpread times the owner's speed over its running speed, plus
 *   JumpingSpread in the air, plus the firing spread (FiringSpreadIncrement a shot, up to FiringSpreadMax, recovering
 *   at FiringSpreadRecovery a second once the trigger is released); times CrouchingSpreadMod crouched.
 * - Recoil: each shot kicks the owner's aim up by RecoilPitch +- RecoilPitchRandom and sideways by +- RecoilYawRandom;
 *   the kick comes back down at RecoilRecovery a second once the trigger is released.
 * - The spread's direction and the recoil come from an FRandomStream seeded with RandomSeed when the weapon spawns,
 *   so a weapon's sequence of shots is the same every time (the tests replay it).
 *
 * Effects: a tracer from the muzzle to the impact, an impact mark on what is not a character, the muzzle flash.
 */
UCLASS(Abstract, Config = Game)
class SHOOTERGAME_API AShooterWeapon_Instant : public AShooterWeapon
{
	GENERATED_BODY()

public:
	AShooterWeapon_Instant(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The damage of a hit at point blank (UE ShooterGame: HitDamage). */
	UPROPERTY(Config)
	float HitDamage = 30.0f;

	/** How far a shot reaches, cm (UE ShooterGame: WeaponRange; CS: 8192 units). */
	UPROPERTY(Config)
	float WeaponRange = 20000.0f;

	/** The damage kept per RangeModifierDistance travelled (CS: the range modifier). */
	UPROPERTY(Config)
	float RangeModifier = 0.98f;

	/** The distance of one range modifier step, cm (CS: 500 units). */
	UPROPERTY(Config)
	float RangeModifierDistance = 1270.0f;

	/** The spread standing still, degrees (UE ShooterGame: WeaponSpread). */
	UPROPERTY(Config)
	float WeaponSpread = 0.3f;

	/** Added at full running speed, in proportion below it, degrees. */
	UPROPERTY(Config)
	float MovingSpread = 3.0f;

	/** Added in the air, degrees. */
	UPROPERTY(Config)
	float JumpingSpread = 6.0f;

	/** The spread's multiplier crouched. */
	UPROPERTY(Config)
	float CrouchingSpreadMod = 0.8f;

	/** Added by each shot, degrees (UE ShooterGame: FiringSpreadIncrement). */
	UPROPERTY(Config)
	float FiringSpreadIncrement = 0.4f;

	/** The most firing spread, degrees (UE ShooterGame: FiringSpreadMax). */
	UPROPERTY(Config)
	float FiringSpreadMax = 4.0f;

	/** Firing spread lost a second once the trigger is released, degrees. */
	UPROPERTY(Config)
	float FiringSpreadRecovery = 6.0f;

	/** The aim's kick a shot, up, and its random part, degrees. */
	UPROPERTY(Config)
	float RecoilPitch = 0.8f;

	UPROPERTY(Config)
	float RecoilPitchRandom = 0.25f;

	/** The sideways kick's range, degrees (either way). */
	UPROPERTY(Config)
	float RecoilYawRandom = 0.4f;

	/** How fast the kick comes back down once the trigger is released, degrees a second. */
	UPROPERTY(Config)
	float RecoilRecovery = 10.0f;

	/** The seed of the weapon's spread and recoil stream. */
	UPROPERTY(Config)
	int32 RandomSeed = 1;

	/** The tracer: its colour (added to the scene), width (cm) and life (seconds). */
	UPROPERTY(Config)
	FLinearColor TracerColor = FLinearColor(3.0f, 2.4f, 1.2f, 1.0f);

	UPROPERTY(Config)
	float TracerWidth = 1.2f;

	UPROPERTY(Config)
	float TracerLifeSpan = 0.05f;

	/** The side of the mark a shot leaves on a surface, cm. */
	UPROPERTY(Config)
	float ImpactMarkSize = 6.0f;

	/** The spread now, degrees (the cone's half angle; see the class comment). */
	[[nodiscard]] virtual float GetCurrentSpread() const;
	/** The accumulated firing spread, degrees. */
	[[nodiscard]] float GetCurrentFiringSpread() const
	{
		return CurrentFiringSpread;
	}
	/** The damage a hit does at Distance cm (the range falloff). */
	[[nodiscard]] float GetDamageAtDistance(float Distance) const;

	/** The last shot: its start, its direction (spread applied) and what it hit. */
	[[nodiscard]] const FVector& GetLastShotStart() const
	{
		return LastShotStart;
	}
	[[nodiscard]] const FVector& GetLastShotDirection() const
	{
		return LastShotDirection;
	}
	[[nodiscard]] const FHitResult& GetLastHit() const
	{
		return LastHit;
	}
	/** The recoil kick not recovered yet, degrees up. */
	[[nodiscard]] float GetRecoilToRecover() const
	{
		return RecoilPitchToRecover;
	}

	void PostInitializeComponents() override;
	void Tick(float DeltaSeconds) override;

protected:
	/** The trace, its damage and effects, then the recoil (UE ShooterGame: FireWeapon / ProcessInstantHit). */
	void FireWeapon() override;
	/** What the shot struck: damage, the hit marker, the impact mark (UE ShooterGame: ProcessInstantHit). */
	void ProcessInstantHit(const FHitResult& Impact, const FVector& Origin, const FVector& ShootDir);
	/** Kicks the owner's aim (see the class comment). */
	void ApplyRecoil();

	/** The spread and recoil stream (UE ShooterGame: a random seed per shot, WeaponRandomStream). */
	FRandomStream WeaponRandomStream;
	float CurrentFiringSpread = 0.0f;
	float RecoilPitchToRecover = 0.0f;
	FVector LastShotStart = FVector::ZeroVector;
	FVector LastShotDirection = FVector::ZeroVector;
	FHitResult LastHit;
};

/** A USP-like pistol: semi-automatic, 12 rounds, accurate standing (CS: USP; every player's first weapon). */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_Pistol : public AShooterWeapon_Instant
{
	GENERATED_BODY()

public:
	AShooterWeapon_Pistol(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** An AK-like rifle: automatic at 600 rounds a minute, 30 rounds, strong against armor (CS: AK-47 / M4A1). */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_Rifle : public AShooterWeapon_Instant
{
	GENERATED_BODY()

public:
	AShooterWeapon_Rifle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
