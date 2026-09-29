#pragma once

#include "CollisionQuery.h"
#include "CoreMinimal.h"
#include "Weapons/ShooterWeapon.h"
#include "ShooterWeapon_Knife.generated.h"

class AShooterCharacter;

/**
 * Counter-Strike's knife: every player's (the Knife slot, key 3), never dropped nor bought, no ammunition
 * (bInfiniteClip). The trigger slashes (SlashDamage within SlashRange, every TimeBetweenShots while held), the
 * secondary button stabs (StabDamage within StabRange; StabCycleTime before the next attack), a stab in the back
 * (IsBackstab: CS's dot product over 0.8) BackstabMultiplier times as hard. A cut is a line from the eyes on the Weapon
 * channel, and a sphere of HullRadius along it when the line misses (CS: a hull trace); the hit character takes point
 * damage, so the hit groups count as for a bullet.
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_Knife : public AShooterWeapon
{
	GENERATED_BODY()

public:
	AShooterWeapon_Knife(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** A slash: its damage and reach, cm (CS: 15, 48 units). */
	UPROPERTY(Config)
	float SlashDamage = 15.0f;

	UPROPERTY(Config)
	float SlashRange = 121.92f;

	/** A stab: its damage, reach (CS: 65, 32 units) and the wait before the next attack (CS: 1.1 s). */
	UPROPERTY(Config)
	float StabDamage = 65.0f;

	UPROPERTY(Config)
	float StabRange = 81.28f;

	UPROPERTY(Config)
	float StabCycleTime = 1.1f;

	/** A stab in the back's multiplier (CS: 3). */
	UPROPERTY(Config)
	float BackstabMultiplier = 3.0f;

	/** The sphere a cut sweeps when its line misses, cm. */
	UPROPERTY(Config)
	float HullRadius = 10.0f;

	/** Stabs (the secondary button) when the knife may attack. */
	void StartSecondaryFire() override;
	/** A stab's wait after a stab, the slash's otherwise. */
	[[nodiscard]] float GetTimeBetweenShots() const override;

	/**
	 * A cut from Attacker's place is in Victim's back (CS: the direction from the attacker to the victim, level, and
	 * the victim's facing agree by more than 0.8).
	 */
	[[nodiscard]] static bool IsBackstab(const FVector& AttackerLocation, const AShooterCharacter& Victim);

	/** The last attack: what it hit (not a blocking hit when it cut the air), whether it was a stab, in the back. */
	[[nodiscard]] const FHitResult& GetLastHit() const
	{
		return LastHit;
	}
	[[nodiscard]] bool WasLastAttackStab() const
	{
		return bLastAttackStab;
	}
	[[nodiscard]] bool WasLastAttackBackstab() const
	{
		return bLastAttackBackstab;
	}

protected:
	/** A slash, or the stab StartSecondaryFire asked for. */
	void FireWeapon() override;
	/** The cut's sound and animation, and a quiet noise; no muzzle flash. */
	void SimulateWeaponFire() override;

private:
	/** The stab asked for by the secondary button (FireWeapon reads it). */
	bool bStabbing = false;
	bool bLastAttackStab = false;
	bool bLastAttackBackstab = false;
	FHitResult LastHit;
};
