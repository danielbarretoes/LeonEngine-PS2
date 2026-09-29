#pragma once

#include "CoreMinimal.h"
#include "Weapons/ShooterWeapon.h"
#include "ShooterWeapon_Projectile.generated.h"

class AShooterProjectile;

/**
 * A weapon that throws projectiles (UE ShooterGame: AShooterWeapon_Projectile, a rocket launcher there): each shot
 * spawns ProjectileClass at the eyes, ahead of the pawn, and launches it along the aim tilted up by ThrowPitch at
 * ThrowSpeed plus the thrower's velocity. The projectile takes the weapon's fuse, damage, radius, armor ratio, mesh and
 * explosion sound. When the last round is thrown the weapon leaves the inventory and is destroyed (a grenade), and the
 * pawn draws its best weapon.
 *
 * The grenades (CS 1.6): the HE, the flashbang and the smoke grenade share the grenade slot, one weapon of each; a
 * weapon holds up to AmmoPerClip of its grenades (the flashbang two, the others one), one to start with, and a
 * purchase of one already carried adds a grenade (AddGrenade). The grenade key cycles them in GrenadeOrder (the HE,
 * the flashbang, the smoke grenade).
 */
UCLASS(Abstract, Config = Game)
class SHOOTERGAME_API AShooterWeapon_Projectile : public AShooterWeapon
{
	GENERATED_BODY()

public:
	AShooterWeapon_Projectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** What a shot throws (UE ShooterGame: ProjectileClass). */
	UPROPERTY()
	TSubclassOf<AShooterProjectile> ProjectileClass;

	/** The throw's speed, cm/s (CS: about 750 units a second). */
	UPROPERTY(Config)
	float ThrowSpeed = 1500.0f;

	/** Degrees the throw is tilted up from the aim (CS aims a grenade a little above the crosshair). */
	UPROPERTY(Config)
	float ThrowPitch = 8.0f;

	/** The projectile's fuse, damage and radius (AShooterProjectile). */
	UPROPERTY(Config)
	float FuseTime = 1.5f;

	UPROPERTY(Config)
	float ExplosionDamage = 98.0f;

	UPROPERTY(Config)
	float ExplosionRadius = 889.0f;

	/** The explosion's sound (a sound wave of /Game/Sounds). */
	UPROPERTY(Config)
	FSoftObjectPath ExplodeSoundName;

	/** Where the grenade key's cycle puts it (the HE 0, the flashbang 1, the smoke grenade 2). */
	UPROPERTY()
	int32 GrenadeOrder = 0;

	/** One more grenade of the kind, up to AmmoPerClip; false when the weapon holds as many as it may. */
	bool AddGrenade();

	/** The last projectile thrown, while it flies. */
	[[nodiscard]] AShooterProjectile* GetLastProjectile() const
	{
		return LastProjectile;
	}

	/** A new grenade weapon holds one grenade. */
	void PostInitializeComponents() override;

protected:
	/** Spawns and launches a projectile (UE ShooterGame: FireWeapon / ServerFireProjectile). */
	void FireWeapon() override;
	/** The throw's sound, no muzzle flash. */
	void SimulateWeaponFire() override;
	/** Out of rounds: leaves the pawn's inventory. */
	void OnShotFired() override;

	UPROPERTY(Transient)
	AShooterProjectile* LastProjectile = nullptr;

	UPROPERTY(Transient)
	USoundWave* ExplodeSound = nullptr;
};

/** Counter-Strike's HE grenade: one, 98 damage within 889 cm after 1.5 s, $300 (CS: hegrenade). */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_HEGrenade : public AShooterWeapon_Projectile
{
	GENERATED_BODY()

public:
	AShooterWeapon_HEGrenade(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Counter-Strike's flashbang: two, a flash after 1.5 s (AShooterProjectile_Flashbang), $200 (CS: flashbang). */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_Flashbang : public AShooterWeapon_Projectile
{
	GENERATED_BODY()

public:
	AShooterWeapon_Flashbang(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Counter-Strike's smoke grenade: one, a cloud after 1.5 s (AShooterProjectile_Smoke), $300 (CS: smokegrenade). */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_SmokeGrenade : public AShooterWeapon_Projectile
{
	GENERATED_BODY()

public:
	AShooterWeapon_SmokeGrenade(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
