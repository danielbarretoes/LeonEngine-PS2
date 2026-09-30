#pragma once

#include "CollisionQuery.h"
#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ShooterTypes.h"
#include "Weapons/ShooterWeapon.h"
#include "ShooterWeapon_Instant.generated.h"

/**
 * A hitscan weapon (UE ShooterGame: AShooterWeapon_Instant): each shot is a line on the Weapon channel from the
 * owner's eyes, within the current spread, out to WeaponRange; what it hits takes point damage
 * (UGameplayStatics::ApplyPointDamage, this weapon the causer).
 *
 * Counter-Strike 1.6's model, in degrees and centimetres (1 CS unit = 2.54 cm):
 * - Damage: HitDamage x RangeModifier^(distance / RangeModifierDistance) (CS: the range modifier per 500 units,
 *   1270 cm). The victim multiplies it by the hit group (AShooterCharacter::GetHitGroup: the head by
 *   HeadshotMultiplier) and applies its armor with ArmorRatio (AShooterCharacter::TakeDamage).
 * - Penetration (CS's FireBullets3): a shot goes through up to PenetrationCount - 1 things. A character lets it through
 *   with 3/4 of the damage (it goes on 107 cm past the entry, CS's 42 units); a surface when the bullet leaves it
 *   within the penetration power (PenetrationPower, cm, cut down by each material it meets for the rest of the shot:
 *   GetSurfacePenetration), keeping the material's share of the damage; the range left halves. Beyond
 *   PenetrationDistance from the shooter nothing is pierced. The surface is the hit's physical material's
 *   (FHitResult::PhysMaterial: the hit mesh's material's, UMaterial::PhysMaterial; GetSurfaceType).
 * - Spread, a cone's half angle (CS's per-weapon cases in each *PrimaryAttack: in the air, running, walking, ducking,
 *   still): WeaponSpread standing still; plus the movement's term, which grows with the owner's speed to WalkingSpread
 *   at WalkingSpeed (CS's 140 units a second, the fastest a walk goes) and on to MovingSpread at the weapon's running
 *   speed (GetMovementSpread); plus JumpingSpread off the floor (in the air, on a ladder); plus the firing spread
 *   (FiringSpreadIncrement a shot, up to FiringSpreadMax, recovering at FiringSpreadRecovery a second once the trigger
 *   is released); all of it times CrouchingSpreadMod crouched (0.5 to 0.65 of standing, by the weapon). So crouched
 *   beats standing, walking beats running and the air is worst; a walk costs the pistols and the AWP accuracy, but not
 *   the rifles and the MP5 (WalkingSpread 0: CS's cases for them only look past 140 units a second). The HUD's
 *   crosshair (AShooterHUD::GetCrosshairGap) follows the spread.
 * - Recoil: each shot kicks the owner's aim up by RecoilPitch +- RecoilPitchRandom and sideways by +- RecoilYawRandom,
 *   scaled by the owner's state (GetRecoilScale: CS's KickBack branches, moving, in the air, crouched); the kick comes
 *   back down at RecoilRecovery a second once the trigger is released.
 * - The spread's direction and the recoil come from an FRandomStream seeded with RandomSeed when the weapon spawns,
 *   so a weapon's sequence of shots is the same every time (the tests replay it).
 * - A silencer (bHasSilencer: the USP, the M4A1): the secondary button puts it on or takes it off in
 *   SilencerDuration (no shot meanwhile). Silenced, a shot does SilencedHitDamage with SilencedRangeModifier, spreads
 *   SilencedSpreadScale times as much, is SilencedFireNoiseLoudness loud to the bots and SilencedFireVolume loud.
 * - A burst mode (bHasBurstMode: the Glock-18): the secondary button toggles it; a press then fires BurstShots shots
 *   BurstShotInterval apart with BurstSpreadScale times the spread, and the next press waits BurstCycleTime from the
 *   first.
 *
 * Effects: a tracer from the muzzle to where the bullet stopped, an impact mark where it entered a surface (its tint
 * and size by the surface, GetImpactMarkStyle), the muzzle flash. Sounds (CS's): a bullet that enters a surface plays
 * that surface's impact sound there (ImpactSounds, a variant after another; `debris/` and the ricochets in CS), one
 * that hits a character the body's (CS's `bhit_`): a helmet on the head (HelmetHitSoundName), armor where it covers
 * (ArmorHitSoundName), else the flesh (ImpactSounds' Flesh).
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
	float WeaponRange = 20808.0f;

	/** The damage kept per RangeModifierDistance travelled (CS: the range modifier). */
	UPROPERTY(Config)
	float RangeModifier = 0.98f;

	/** The distance of one range modifier step, cm (CS: 500 units). */
	UPROPERTY(Config)
	float RangeModifierDistance = 1270.0f;

	/** How many things a shot may hit: 1 pierces nothing (CS: iPenetration, 2 for the rifles, 3 for the AWP). */
	UPROPERTY(Config)
	int32 PenetrationCount = 1;

	/** How deep into a surface the bullet reaches to leave it, cm (CS: the bullet's penetration power in units). */
	UPROPERTY(Config)
	float PenetrationPower = 0.0f;

	/** Beyond this from the shooter a bullet pierces nothing, cm (CS: the bullet's penetration distance). */
	UPROPERTY(Config)
	float PenetrationDistance = 0.0f;

	/** The spread standing still, degrees (UE ShooterGame: WeaponSpread). */
	UPROPERTY(Config)
	float WeaponSpread = 0.3f;

	/** Added at WalkingSpeed, in proportion below it, degrees. */
	UPROPERTY(Config)
	float WalkingSpread = 1.0f;

	/** The fastest a walk goes, cm/s (CS: 140 units a second); past it the owner runs. */
	UPROPERTY(Config)
	float WalkingSpeed = 355.6f;

	/** Added at the weapon's full running speed, degrees; from WalkingSpread at WalkingSpeed, in proportion. */
	UPROPERTY(Config)
	float MovingSpread = 3.0f;

	/** Added off the floor (in the air, on a ladder), degrees. */
	UPROPERTY(Config)
	float JumpingSpread = 6.0f;

	/** The spread's multiplier crouched. */
	UPROPERTY(Config)
	float CrouchingSpreadMod = 0.6f;

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

	/**
	 * The kick's share by the owner's state (CS's KickBack branches: the rifles' and the MP5's; the rest keep the
	 * standing kick): moving at all (CS: speed > 0), off the floor, crouched.
	 */
	UPROPERTY(Config)
	FShooterRecoilScale MovingRecoilScale;

	UPROPERTY(Config)
	FShooterRecoilScale JumpingRecoilScale;

	UPROPERTY(Config)
	FShooterRecoilScale CrouchingRecoilScale;

	/** CS's AK-47 and M4A1 test the movement before the air (moving in the air kicks as moving); the MP5 the other way.
	 */
	UPROPERTY(Config)
	bool bRecoilMovingBeforeAir = false;

	/** How fast the kick comes back down once the trigger is released, degrees a second. */
	UPROPERTY(Config)
	float RecoilRecovery = 10.0f;

	/** The silencer (see the class comment). */
	UPROPERTY(Config)
	bool bHasSilencer = false;

	UPROPERTY(Config)
	float SilencerDuration = 3.0f;

	UPROPERTY(Config)
	float SilencedHitDamage = 0.0f;

	UPROPERTY(Config)
	float SilencedRangeModifier = 0.0f;

	UPROPERTY(Config)
	float SilencedSpreadScale = 1.0f;

	UPROPERTY(Config)
	float SilencedFireNoiseLoudness = 0.2f;

	UPROPERTY(Config)
	float SilencedFireVolume = 0.35f;

	/** The burst mode (see the class comment). */
	UPROPERTY(Config)
	bool bHasBurstMode = false;

	UPROPERTY(Config)
	int32 BurstShots = 3;

	UPROPERTY(Config)
	float BurstShotInterval = 0.1f;

	UPROPERTY(Config)
	float BurstCycleTime = 0.5f;

	UPROPERTY(Config)
	float BurstSpreadScale = 1.0f;

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

	/** The side of the mark a shot leaves on a surface, cm (GetImpactMarkStyle scales it by the surface). */
	UPROPERTY(Config)
	float ImpactMarkSize = 6.0f;

	/**
	 * Each surface's impact sounds (every hitscan weapon's: `ImpactSounds=(...)` in
	 * `[/Script/ShooterGame.ShooterWeapon_Instant]`); Flesh is a character's hit without armor.
	 */
	UPROPERTY(GlobalConfig)
	FShooterSurfaceSounds ImpactSounds;

	/** A hit on armor (CS: `bhit_kevlar`) and on a helmet (CS: `bhit_helmet`). */
	UPROPERTY(GlobalConfig)
	FSoftObjectPath ArmorHitSoundName;

	UPROPERTY(GlobalConfig)
	FSoftObjectPath HelmetHitSoundName;

	/**
	 * What the surface a hit struck is made of: its physical material's surface (the trace asks for it,
	 * FCollisionQueryParams::bReturnPhysicalMaterial); SHOOTER_SURFACE_Default without one.
	 */
	[[nodiscard]] static EPhysicalSurface GetSurfaceType(const FHitResult& Hit);
	/**
	 * Counter-Strike's penetration of a surface (FireBullets3's texture types): the share of the penetration power it
	 * leaves (for the rest of the shot) and of the damage (concrete 0.25 and 0.5, wood 1 and 0.6, metal 0.15 and 0.2,
	 * tile 0.65 and 0.2, computer 0.4 and 0.45; dirt, glass, flesh and the default 1 and 0.5).
	 */
	static void GetSurfacePenetration(EPhysicalSurface Surface, float& OutPowerScale, float& OutDamageScale);
	/**
	 * The mark a bullet leaves on a surface: its tint and its size's scale (dark on concrete and tile, brown and
	 * larger in dirt, dark brown in wood, small and grey on metal, pale on glass).
	 */
	static void GetImpactMarkStyle(EPhysicalSurface Surface, FLinearColor& OutColor, float& OutSizeScale);
	/**
	 * The sound of a hit: on a character (the victim's armor and helmet before the hit decide), else the surface's
	 * impact sound, variant Variant; null for none.
	 */
	[[nodiscard]] USoundWave* GetImpactSound(const FHitResult& Impact, int32 Variant) const;
	/** The sound the last hit of a shot played (the tests). */
	[[nodiscard]] USoundWave* GetLastImpactSound() const
	{
		return LastImpactSound;
	}

	/** The spread now, degrees (the cone's half angle; see the class comment). */
	[[nodiscard]] virtual float GetCurrentSpread() const;
	/**
	 * The movement's share of the spread at Speed cm/s, degrees: 0 still, WalkingSpread at WalkingSpeed, MovingSpread
	 * at RunSpeed (the owner's running speed with this weapon), in proportion between them.
	 */
	[[nodiscard]] float GetMovementSpread(float Speed, float RunSpeed) const;
	/** The share of the next shot's kick by the owner's state now (see MovingRecoilScale); 1 and 1 without an owner. */
	[[nodiscard]] FShooterRecoilScale GetRecoilScale() const;
	/** The accumulated firing spread, degrees. */
	[[nodiscard]] float GetCurrentFiringSpread() const
	{
		return CurrentFiringSpread;
	}
	/** The damage a hit does at Distance cm (the range falloff; silenced, the silenced damage and falloff). */
	[[nodiscard]] float GetDamageAtDistance(float Distance) const;

	/** The silencer is on. */
	[[nodiscard]] bool IsSilenced() const
	{
		return bSilenced;
	}
	/** Puts the silencer on or takes it off at once (a weapon with one; the tests). */
	void SetSilenced(bool bNewSilenced);
	/** The burst mode is on. */
	[[nodiscard]] bool IsBurstMode() const
	{
		return bBurstMode;
	}
	/** The secondary button: the silencer (in SilencerDuration) or the burst mode, when the weapon has one. */
	void StartSecondaryFire() override;
	[[nodiscard]] float GetTimeBetweenShots() const override;
	[[nodiscard]] float GetFireNoiseLoudness() const override;

	/** The last shot: its start, its direction (spread applied) and the first thing it hit. */
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
	/** Where the last shot stopped (the tracer's end), and how many things it went through. */
	[[nodiscard]] const FVector& GetLastShotEnd() const
	{
		return LastShotEnd;
	}
	[[nodiscard]] int32 GetLastShotPenetrations() const
	{
		return LastShotPenetrations;
	}
	/** The recoil kick not recovered yet, degrees up. */
	[[nodiscard]] float GetRecoilToRecover() const
	{
		return RecoilPitchToRecover;
	}
	/**
	 * The holder pulls the kick down by up to Degrees (a bot's recoil control): the owner's aim goes down that much and
	 * no longer has it to recover.
	 */
	void CompensateRecoil(float Degrees);

	/** The firing spread and the recoil to recover go back to 0. */
	void ResetAim() override;
	/** A burst stops with the weapon. */
	void OnUnEquip() override;
	void PostInitializeComponents() override;
	void Tick(float DeltaSeconds) override;

protected:
	/** The shot, its damage and effects, then the recoil (UE ShooterGame: FireWeapon / ProcessInstantHit). */
	void FireWeapon() override;
	/** A burst's first shot schedules the rest. */
	void OnShotFired() override;
	[[nodiscard]] float GetFireVolume() const override;
	/**
	 * What one segment of the shot struck (UE ShooterGame: ProcessInstantHit): Damage to an actor that can be damaged
	 * (the hit marker for a player's hit on a character), a mark on a surface; returns the damage the actor took.
	 */
	float ProcessInstantHit(const FHitResult& Impact, const FVector& ShootDir, float Damage);
	/**
	 * Where a bullet that entered Entry's component along Direction leaves it within Depth cm (a line back from Depth
	 * ahead finds the component's far side); false when it does not (thicker, or the end is still inside).
	 */
	bool FindPenetrationExit(const FHitResult& Entry, const FVector& Direction, float Depth,
		const FCollisionQueryParams& Params, FVector& OutExit) const;
	/** Kicks the owner's aim (see the class comment). */
	void ApplyRecoil();

	/** The impact sounds (ImpactSounds, ArmorHitSoundName, HelmetHitSoundName), loaded. */
	UPROPERTY(Transient)
	FShooterSurfaceSoundSet ImpactSoundSet;

	UPROPERTY(Transient)
	USoundWave* ArmorHitSound = nullptr;

	UPROPERTY(Transient)
	USoundWave* HelmetHitSound = nullptr;

	UPROPERTY(Transient)
	USoundWave* LastImpactSound = nullptr;

	/** The impacts so far: the variant of the next impact sound. */
	int32 NumImpacts = 0;

	/** The spread and recoil stream (UE ShooterGame: a random seed per shot, WeaponRandomStream). */
	FRandomStream WeaponRandomStream;
	float CurrentFiringSpread = 0.0f;
	float RecoilPitchToRecover = 0.0f;
	FVector LastShotStart = FVector::ZeroVector;
	FVector LastShotDirection = FVector::ZeroVector;
	FVector LastShotEnd = FVector::ZeroVector;
	int32 LastShotPenetrations = 0;
	FHitResult LastHit;
	bool bSilenced = false;
	bool bBurstMode = false;
	/** The burst's shots still to come, and the world time of the next one. */
	int32 BurstShotsLeft = 0;
	float NextBurstShotTime = 0.0f;
};

/** Counter-Strike's Glock-18: the terrorists' first pistol, 20 rounds, a three-round burst mode. */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_Glock : public AShooterWeapon_Instant
{
	GENERATED_BODY()

public:
	AShooterWeapon_Glock(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Counter-Strike's USP: the counter-terrorists' first pistol, 12 rounds, a silencer. */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_USP : public AShooterWeapon_Instant
{
	GENERATED_BODY()

public:
	AShooterWeapon_USP(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Counter-Strike's Desert Eagle: 7 heavy rounds that go through a wall. */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_Deagle : public AShooterWeapon_Instant
{
	GENERATED_BODY()

public:
	AShooterWeapon_Deagle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Counter-Strike's MP5: a cheap sub-machine gun for both teams, accurate on the move. */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_MP5 : public AShooterWeapon_Instant
{
	GENERATED_BODY()

public:
	AShooterWeapon_MP5(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Counter-Strike's AK-47: the terrorists' rifle, strong against armor. */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_AK47 : public AShooterWeapon_Instant
{
	GENERATED_BODY()

public:
	AShooterWeapon_AK47(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Counter-Strike's M4A1: the counter-terrorists' rifle, with a silencer. */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterWeapon_M4A1 : public AShooterWeapon_Instant
{
	GENERATED_BODY()

public:
	AShooterWeapon_M4A1(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
