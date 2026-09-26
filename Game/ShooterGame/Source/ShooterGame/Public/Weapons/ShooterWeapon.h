#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterTypes.h"
#include "UObject/SoftObjectPath.h"
#include "ShooterWeapon.generated.h"

class AController;
class AShooterCharacter;
class USoundWave;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A weapon a ShooterGame character carries (UE ShooterGame: AShooterWeapon): its slot, its ammunition (a clip and a
 * reserve), the firing loop (fire rate, automatic or semi-automatic, reloads, equipping) and its looks. The subclasses
 * fire: AShooterWeapon_Instant traces bullets (pistols, rifles, AShooterWeapon_Sniper), AShooterWeapon_Projectile
 * throws projectiles (the HE grenade).
 *
 * Tuning is config (UPROPERTY(Config)): each weapon class has its section in DefaultGame.ini, e.g.
 * `[/Script/ShooterGame.ShooterWeapon_Rifle]`, over the defaults its constructor sets.
 *
 * Looks: two static mesh components show MeshName. Mesh1P is the first-person view model (bRenderAsViewModel,
 * bOnlyOwnerSee: only its owner's view draws it, after the world, with the camera's ViewModelFOV), attached to the
 * pawn's first-person camera at FirstPersonOffset; Mesh3P is what the others see (bOwnerNoSee), attached to the pawn's
 * body at ThirdPersonOffset. Only the equipped weapon shows. The muzzle is the mesh's `Muzzle` socket (MuzzleOffset
 * in the mesh's space without one).
 *
 * Timing: Leon has no timer manager, so the weapon counts in its tick against the world's time: a shot every
 * TimeBetweenShots while the trigger is held (automatic) or once per press (semi-automatic), EquipDuration before the
 * first shot, ReloadDuration for a reload. Firing with an empty clip reloads when there is reserve ammo, else clicks.
 */
UCLASS(Abstract, Config = Game)
class SHOOTERGAME_API AShooterWeapon : public AActor
{
	GENERATED_BODY()

public:
	AShooterWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The name the kill feed, the HUD and `give` use (CS's names: usp, ak47, awp, hegrenade). */
	UPROPERTY(Config)
	FString WeaponName;

	/** The inventory slot (set by each class). */
	UPROPERTY()
	EShooterWeaponSlot Slot = EShooterWeaponSlot::Primary;

	/** Rounds in a full clip (UE ShooterGame: AmmoPerClip). */
	UPROPERTY(Config)
	int32 AmmoPerClip = 30;

	/** The most rounds carried besides the clip (Leon: UE ShooterGame's MaxAmmo counts the clip too). */
	UPROPERTY(Config)
	int32 MaxAmmo = 90;

	/** Seconds between two shots (UE ShooterGame: TimeBetweenShots; 60 / rounds per minute). */
	UPROPERTY(Config)
	float TimeBetweenShots = 0.1f;

	/** Keeps firing while the trigger is held; else one shot a press. */
	UPROPERTY(Config)
	bool bAutomatic = true;

	/** Seconds a reload takes (UE ShooterGame: NoAnimReloadDuration). */
	UPROPERTY(Config)
	float ReloadDuration = 2.5f;

	/** Seconds from drawing the weapon to its first shot (UE ShooterGame: NoEquipAnimDuration; CS's deploy time). */
	UPROPERTY(Config)
	float EquipDuration = 1.0f;

	/** The multiplier of a hit in the head (CS: 4). */
	UPROPERTY(Config)
	float HeadshotMultiplier = 4.0f;

	/**
	 * Counter-Strike's armor ratio: an armored victim takes ArmorRatio / 2 of the damage to its health, and its armor
	 * loses half of the rest (AShooterCharacter::TakeDamage). 1 halves the damage; above 2 armor does not help.
	 */
	UPROPERTY(Config)
	float ArmorRatio = 1.0f;

	/**
	 * The carrier's speed with the weapon drawn, a fraction of the running speed (CS: 250 units a second with a knife
	 * or a pistol, 221 with an AK-47, 210 with an AWP).
	 */
	UPROPERTY(Config)
	float SpeedModifier = 1.0f;

	/** What the weapon costs in the buy menu (P19). */
	UPROPERTY(Config)
	int32 Price = 0;

	/** The money a kill with it pays (P19; CS: 300, the AWP 100). */
	UPROPERTY(Config)
	int32 KillReward = 300;

	/** The weapon's mesh (both views and the pickup): a static mesh of /Game/Weapons. */
	UPROPERTY(Config)
	FSoftObjectPath MeshName;

	/** Where Mesh1P sits in the first-person camera's space (forward, right, up), cm. */
	UPROPERTY(Config)
	FVector FirstPersonOffset = FVector(28.0f, 12.0f, -14.0f);

	/** Where Mesh3P sits in the body's space (the body faces +X), cm. */
	UPROPERTY(Config)
	FVector ThirdPersonOffset = FVector(24.0f, 16.0f, 128.0f);

	/** The muzzle in the mesh's space when the mesh has no `Muzzle` socket, cm. */
	UPROPERTY(Config)
	FVector MuzzleOffset = FVector(40.0f, 0.0f, 4.0f);

	/** Sounds: a shot, a reload, an empty click, drawing the weapon (sound waves of /Game/Sounds). */
	UPROPERTY(Config)
	FSoftObjectPath FireSoundName;

	UPROPERTY(Config)
	FSoftObjectPath ReloadSoundName;

	UPROPERTY(Config)
	FSoftObjectPath EmptySoundName;

	UPROPERTY(Config)
	FSoftObjectPath EquipSoundName;

	/** The socket of the muzzle on the weapon's mesh (UE ShooterGame: MuzzleAttachPoint). */
	static const FName MuzzleSocketName;

	/** The pawn that carries the weapon, or null (UE ShooterGame: GetPawnOwner). */
	[[nodiscard]] AShooterCharacter* GetPawnOwner() const
	{
		return MyPawn;
	}
	/** The owner's controller, or null. */
	[[nodiscard]] AController* GetInstigatorController() const;

	/** Joins a pawn's inventory: owned by it, hidden until equipped (UE ShooterGame: OnEnterInventory). */
	virtual void OnEnterInventory(AShooterCharacter* NewOwner);
	/** Leaves it: unequipped, detached, no owner (UE ShooterGame: OnLeaveInventory). */
	virtual void OnLeaveInventory();
	/** Drawn: shown on the pawn, ready after EquipDuration (UE ShooterGame: OnEquip). */
	virtual void OnEquip();
	/** Put away: the trigger, the reload and the zoom stop, the meshes hide (UE ShooterGame: OnUnEquip). */
	virtual void OnUnEquip();
	[[nodiscard]] bool IsEquipped() const
	{
		return bIsEquipped;
	}

	/** The trigger (UE ShooterGame: StartFire / StopFire). */
	virtual void StartFire();
	virtual void StopFire();
	/** The secondary button: a sniper's zoom; nothing by default (UE ShooterGame: the pawn's targeting). */
	virtual void StartSecondaryFire()
	{
	}
	/** Starts a reload when CanReload (UE ShooterGame: StartReload). */
	virtual void StartReload();
	/** Cancels a reload (UE ShooterGame: StopReload). */
	void StopReload();

	/** Equipped, not reloading, ready (UE ShooterGame: CanFire). */
	[[nodiscard]] bool CanFire() const;
	/** Equipped, the clip not full and reserve ammo left (UE ShooterGame: CanReload). */
	[[nodiscard]] bool CanReload() const;

	/** UE ShooterGame: GetCurrentState. */
	[[nodiscard]] EShooterWeaponState GetCurrentState() const
	{
		return CurrentState;
	}
	/** Rounds in the reserve (UE ShooterGame: GetCurrentAmmo, which counts the clip too). */
	[[nodiscard]] int32 GetCurrentAmmo() const
	{
		return CurrentAmmo;
	}
	[[nodiscard]] int32 GetCurrentAmmoInClip() const
	{
		return CurrentAmmoInClip;
	}
	/** Adds rounds to the reserve, up to MaxAmmo; returns how many it took (UE ShooterGame: GiveAmmo). */
	int32 GiveAmmo(int32 AddAmount);
	/** Fills the clip and the reserve (a new weapon, a round's restart). */
	void RefillAmmo();
	/** The carrier's speed modifier now (SpeedModifier; a scoped sniper is slower). */
	[[nodiscard]] virtual float GetSpeedModifier() const
	{
		return SpeedModifier;
	}
	/**
	 * The weapon class `give` and the buy menu name: a class whose WeaponName is Name, or whose class name ends with it
	 * (ShooterWeapon_Rifle: "rifle"), any case; null when none matches.
	 */
	[[nodiscard]] static UClass* FindWeaponClass(const FString& Name);

	/** Shots fired since it was spawned. */
	[[nodiscard]] int32 GetShotsFired() const
	{
		return ShotsFired;
	}

	/**
	 * The shot's start and direction before any spread: the owner's first-person camera and its control rotation (UE
	 * ShooterGame: GetCameraDamageStartLocation / GetAdjustedAim); the weapon's own place and facing without an owner.
	 */
	void GetAim(FVector& OutStart, FVector& OutDirection) const;
	/** The muzzle in the world: the socket of the mesh the viewer sees (UE ShooterGame: GetMuzzleLocation). */
	[[nodiscard]] FVector GetMuzzleLocation() const;

	/** The first-person and third-person meshes (UE ShooterGame: Mesh1P, Mesh3P). */
	[[nodiscard]] UStaticMeshComponent* GetMesh1P() const
	{
		return Mesh1P;
	}
	[[nodiscard]] UStaticMeshComponent* GetMesh3P() const
	{
		return Mesh3P;
	}
	/** The weapon's mesh asset, if loaded. */
	[[nodiscard]] UStaticMesh* GetWeaponMesh() const;

	void PostInitializeComponents() override;
	void Tick(float DeltaSeconds) override;

protected:
	/** Fires one shot: the subclass's trace or projectile (UE ShooterGame: FireWeapon). */
	virtual void FireWeapon()
	{
	}
	/** The shot's looks and sound: the muzzle light and the fire sound (UE ShooterGame: SimulateWeaponFire). */
	virtual void SimulateWeaponFire();
	/** After a shot (a sniper leaves its zoom). */
	virtual void OnShotFired()
	{
	}
	/** Moves rounds from the reserve into the clip (UE ShooterGame: ReloadWeapon). */
	virtual void ReloadWeapon();
	/** Takes one round from the clip (UE ShooterGame: UseAmmo). */
	void UseAmmo();
	/** One trigger pull: a shot, or a reload or a click on an empty clip (UE ShooterGame: HandleFiring). */
	void HandleFiring();
	/** Attaches the meshes to the owner and shows the equipped one (UE ShooterGame: AttachMeshToPawn). */
	void AttachMeshToPawn();
	/** Hides and detaches the meshes (UE ShooterGame: DetachMeshFromPawn). */
	void DetachMeshFromPawn();
	/** Plays a sound at the weapon (UE ShooterGame: PlayWeaponSound). */
	void PlayWeaponSound(USoundWave* Sound) const;
	/** The world's time (UWorld::GetTimeSeconds). */
	[[nodiscard]] float GetWorldTime() const;

	/** UE ShooterGame: MyPawn. */
	UPROPERTY(Transient)
	AShooterCharacter* MyPawn = nullptr;

	UPROPERTY()
	UStaticMeshComponent* Mesh1P = nullptr;

	UPROPERTY()
	UStaticMeshComponent* Mesh3P = nullptr;

	/** The loaded sounds (null when the asset is missing: silent). */
	UPROPERTY(Transient)
	USoundWave* FireSound = nullptr;

	UPROPERTY(Transient)
	USoundWave* ReloadSound = nullptr;

	UPROPERTY(Transient)
	USoundWave* EmptySound = nullptr;

	UPROPERTY(Transient)
	USoundWave* EquipSound = nullptr;

	EShooterWeaponState CurrentState = EShooterWeaponState::Idle;
	int32 CurrentAmmo = 0;
	int32 CurrentAmmoInClip = 0;
	int32 ShotsFired = 0;
	bool bIsEquipped = false;
	bool bWantsToFire = false;
	/** A semi-automatic weapon fired for this press. */
	bool bFiredThisPress = false;
	/** The world time of the last shot, and when the equip and the reload end (UE ShooterGame's timers). */
	float LastFireTime = -1.0e6f;
	float EquipFinishTime = 0.0f;
	float ReloadFinishTime = 0.0f;
};
