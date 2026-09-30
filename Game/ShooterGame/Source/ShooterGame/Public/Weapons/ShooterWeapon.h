#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterTypes.h"
#include "UObject/SoftObjectPath.h"
#include "ShooterWeapon.generated.h"

class AController;
class AShooterCharacter;
class UAimOffsetBlendSpace1D;
class UAnimMontage;
class UBlendSpaceBase;
class USoundWave;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A weapon a ShooterGame character carries (UE ShooterGame: AShooterWeapon): its slot, its ammunition (a clip and a
 * reserve), the firing loop (fire rate, automatic or semi-automatic, reloads, equipping) and its looks. The subclasses
 * fire: AShooterWeapon_Instant traces bullets (the pistols, the MP5, the rifles and AShooterWeapon_AWP),
 * AShooterWeapon_Knife cuts, AShooterWeapon_Projectile throws projectiles (the HE grenade).
 *
 * Tuning is config (UPROPERTY(Config)): each weapon class has its section in DefaultGame.ini, e.g.
 * `[/Script/ShooterGame.ShooterWeapon_AK47]`, over the defaults its constructor sets (Counter-Strike 1.6's values).
 *
 * Ammunition (CS 1.6): a weapon comes with a full clip and an empty reserve (a pawn's first pistol with two clips more,
 * AShooterCharacter::DefaultWeaponClips); the reserve is bought by the box (AmmoBoxRounds for AmmoBoxPrice, the buy
 * menu's primary and secondary ammo). bInfiniteClip (the knife) needs none.
 *
 * Looks: two static mesh components. Mesh1P shows FirstPersonMeshName, the first-person view model (bRenderAsViewModel,
 * bOnlyOwnerSee: only its owner's view draws it, after the world, with the camera's ViewModelFOV), attached to the
 * pawn's first-person arms' hand socket, or to its camera at FirstPersonOffset without arms; Mesh3P shows MeshName,
 * what the others see (bOwnerNoSee), on the hand socket of the pawn's skinned body, or on its capsule at
 * ThirdPersonOffset without one (AShooterCharacter::GetWeaponAttachParent1P / 3P). Only the equipped weapon shows;
 * drawn, it sets the arms' idle pose (ArmsIdleName) and the body's stance (AimOffsetName). The muzzle is the mesh's
 * `Muzzle` socket (MuzzleOffset in the mesh's space without one).
 *
 * Animations (Docs/PLANS/ps2-shipping.md N25): FireAnim, ReloadAnim and EquipAnim (montages of the arms and the body,
 * from the config's names) play on the pawn when the weapon fires, reloads and is drawn. A draw lasts its montage's
 * length when it has one (UE ShooterGame), EquipDuration otherwise; a reload lasts ReloadDuration (CS's time), its
 * montage played at the rate that fits it (the pistols share one clip, the rifles and the MP5 another). The pawn
 * forwards its animations' notifies (OnAnimNotify): MagOut and MagIn play MagOutSound and MagInSound.
 *
 * On the floor (OnDropped): a weapon a pawn dropped or left when it died lies where it fell, Mesh3P shown, among the
 * game mode's pickups, and is picked up (AShooterCharacter::PickUpWeapon: the draw's sound and the HUD's notice) by the
 * first live pawn of the game mode's pawns that walks within PickupRadius and has its slot free, after PickupDelay (so
 * the pawn that dropped it does not take it back at once). It keeps its ammunition and its state (the silencer, the
 * burst mode).
 *
 * Timing: the fire rate counts in the weapon's tick against the world's time: a shot every GetTimeBetweenShots while
 * the trigger is held (automatic) or once per press (semi-automatic); the draw and the reload are timers. Firing with
 * an empty clip reloads when there is reserve ammo, else clicks.
 */
UCLASS(Abstract, Config = Game)
class SHOOTERGAME_API AShooterWeapon : public AActor
{
	GENERATED_BODY()

public:
	AShooterWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The name the kill feed, the HUD, the buy menu and `give` use (CS's buy names: glock, usp, ak47, awp, ...). */
	UPROPERTY(Config)
	FString WeaponName;

	/** The inventory slot (set by each class). */
	UPROPERTY()
	EShooterWeaponSlot Slot = EShooterWeaponSlot::Primary;

	/** The team that may buy it (CS: the AK-47 the terrorists', the M4A1 the counter-terrorists'); None: both. */
	UPROPERTY(Config)
	EShooterTeam BuyTeam = EShooterTeam::None;

	/** Rounds in a full clip (UE ShooterGame: AmmoPerClip). */
	UPROPERTY(Config)
	int32 AmmoPerClip = 30;

	/** The most rounds carried besides the clip (Leon: UE ShooterGame's MaxAmmo counts the clip too). */
	UPROPERTY(Config)
	int32 MaxAmmo = 90;

	/** Never reloads nor runs out (UE ShooterGame: bInfiniteClip; the knife). */
	UPROPERTY(Config)
	bool bInfiniteClip = false;

	/** A box of the weapon's ammunition: the rounds it adds to the reserve and its price (CS 1.6's, per calibre). */
	UPROPERTY(Config)
	int32 AmmoBoxRounds = 30;

	UPROPERTY(Config)
	int32 AmmoBoxPrice = 0;

	/** Seconds between two shots (UE ShooterGame: TimeBetweenShots; 60 / rounds per minute). */
	UPROPERTY(Config)
	float TimeBetweenShots = 0.1f;

	/** Keeps firing while the trigger is held; else one shot a press. */
	UPROPERTY(Config)
	bool bAutomatic = true;

	/** Seconds a reload takes (CS's time; the reload montage is fitted to it). */
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
	 * The carrier's speed with the weapon drawn, a fraction of the running speed (CS: 250 units a second with a knife,
	 * a pistol or the MP5, 230 with the M4A1, 221 with an AK-47, 210 with an AWP).
	 */
	UPROPERTY(Config)
	float SpeedModifier = 1.0f;

	/** How loud a shot is to the bots (AActor::MakeNoise's loudness). */
	UPROPERTY(Config)
	float FireNoiseLoudness = 1.0f;

	/** What the weapon costs in the buy menu (P19); 0 or less: not for sale (the knife). */
	UPROPERTY(Config)
	int32 Price = 0;

	/** The money a kill with it pays (CS 1.6: $300 with every weapon; per weapon here, config). */
	UPROPERTY(Config)
	int32 KillReward = 300;

	/** How near a pawn's feet a weapon on the floor must be to be picked up, cm (horizontally; 1 m vertically). */
	UPROPERTY(Config)
	float PickupRadius = 60.0f;

	/** Seconds after a drop before the weapon can be picked up. */
	UPROPERTY(Config)
	float PickupDelay = 1.0f;

	/** The weapon's world model (the others' view, the pickup, a thrown grenade): a static mesh of /Game/Weapons. */
	UPROPERTY(Config)
	FSoftObjectPath MeshName;

	/** The first-person model (SM_<Weapon>_1P, more detailed); empty: MeshName in both views. */
	UPROPERTY(Config)
	FSoftObjectPath FirstPersonMeshName;

	/**
	 * The arms' pose while it is drawn (a one-sample blend space of its idle loop: BS_<Weapon>_Idle), and the body's
	 * stance (its aim offset: AO_Rifle, AO_Pistol, AO_Grenade); empty: the arms' reference pose, the character's aim
	 * offset.
	 */
	UPROPERTY(Config)
	FSoftObjectPath ArmsIdleName;

	UPROPERTY(Config)
	FSoftObjectPath AimOffsetName;

	/** Where Mesh1P sits in the first-person camera's space (forward, right, up), cm. */
	UPROPERTY(Config)
	FVector FirstPersonOffset = FVector(28.0f, 12.0f, -14.0f);

	/** Where Mesh3P sits in the body's space (the body faces +X), cm. */
	UPROPERTY(Config)
	FVector ThirdPersonOffset = FVector(24.0f, 16.0f, 128.0f);

	/** The muzzle in the mesh's space when the mesh has no `Muzzle` socket, cm. */
	UPROPERTY(Config)
	FVector MuzzleOffset = FVector(40.0f, 0.0f, 4.0f);

	/** The montages of the arms (1P) and the body (3P): firing, reloading, drawing (N27's art; empty until then). */
	UPROPERTY(Config)
	FSoftObjectPath FireAnim1PName;

	UPROPERTY(Config)
	FSoftObjectPath FireAnim3PName;

	UPROPERTY(Config)
	FSoftObjectPath ReloadAnim1PName;

	UPROPERTY(Config)
	FSoftObjectPath ReloadAnim3PName;

	UPROPERTY(Config)
	FSoftObjectPath EquipAnim1PName;

	UPROPERTY(Config)
	FSoftObjectPath EquipAnim3PName;

	/** The loaded montages (UE ShooterGame: FireAnim, ReloadAnim, EquipAnim); the tests set them too. */
	UPROPERTY(Transient)
	FShooterWeaponAnim FireAnim;

	UPROPERTY(Transient)
	FShooterWeaponAnim ReloadAnim;

	UPROPERTY(Transient)
	FShooterWeaponAnim EquipAnim;

	/** The magazine's sounds, played at the reload montage's MagOut and MagIn notifies. */
	UPROPERTY(Config)
	FSoftObjectPath MagOutSoundName;

	UPROPERTY(Config)
	FSoftObjectPath MagInSoundName;

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
	/** Lies on the floor at Location (its feet), turned to Yaw, until picked up (see the class comment). */
	virtual void OnDropped(const FVector& Location, float Yaw);
	/** On the floor, waiting for a pawn. */
	[[nodiscard]] bool IsDropped() const
	{
		return bDropped;
	}
	/** Drawn: shown on the pawn, ready after EquipDuration (UE ShooterGame: OnEquip). */
	virtual void OnEquip();
	/** Put away: the trigger, the reload and the zoom stop, the meshes hide (UE ShooterGame: OnUnEquip). */
	virtual void OnUnEquip();
	/** The draw's sound at the weapon (OnEquip's; a pickup's too, AShooterCharacter::PickUpWeapon). */
	void PlayEquipSound() const
	{
		PlayWeaponSound(EquipSound);
	}
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
	/** A new round: the accuracy and the aim start clean (the recoil not recovered yet is forgotten). */
	virtual void ResetAim()
	{
	}

	/** Equipped, its pawn alive and not frozen, not reloading, ready (UE ShooterGame: CanFire). */
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
	/**
	 * Any ammunition at all: rounds in the clip or in the reserve (an empty clip with a reserve reloads), or an
	 * infinite clip. A weapon without any is passed over when the best weapon is drawn
	 * (AShooterCharacter::EquipBestWeapon).
	 */
	[[nodiscard]] bool HasAmmo() const
	{
		return bInfiniteClip || CurrentAmmoInClip > 0 || CurrentAmmo > 0;
	}
	/** The reserve has room for more rounds (a box of ammunition can be bought). */
	[[nodiscard]] bool NeedsAmmo() const
	{
		return !bInfiniteClip && CurrentAmmo < MaxAmmo;
	}
	/** Adds rounds to the reserve, up to MaxAmmo; returns how many it took (UE ShooterGame: GiveAmmo). */
	int32 GiveAmmo(int32 AddAmount);
	/** Fills the clip and the reserve (the `give` cheat). */
	void RefillAmmo();
	/** Sets the rounds in the clip and in the reserve, clamped to AmmoPerClip and MaxAmmo (tests, a spent weapon). */
	void SetAmmo(int32 NewAmmoInClip, int32 NewAmmo);
	/** The carrier's speed modifier now (SpeedModifier; a scoped AWP is slower). */
	[[nodiscard]] virtual float GetSpeedModifier() const
	{
		return SpeedModifier;
	}
	/** Seconds from a shot to the next (TimeBetweenShots; a Glock's burst, a knife's stab take longer). */
	[[nodiscard]] virtual float GetTimeBetweenShots() const
	{
		return TimeBetweenShots;
	}
	/** How loud a shot is to the bots now (FireNoiseLoudness; silenced, much less). */
	[[nodiscard]] virtual float GetFireNoiseLoudness() const
	{
		return FireNoiseLoudness;
	}
	/** A player of Team may buy it: it has a price and BuyTeam is None or Team. */
	[[nodiscard]] bool CanBeBoughtBy(EShooterTeam Team) const
	{
		return Price > 0 && (BuyTeam == EShooterTeam::None || BuyTeam == Team);
	}
	/** It can be dropped (G, a death): every weapon but the knife and the grenades. */
	[[nodiscard]] bool CanBeDropped() const
	{
		return Slot == EShooterWeaponSlot::Primary || Slot == EShooterWeaponSlot::Secondary;
	}
	/**
	 * The weapon class `give` and the buy menu name: a class whose WeaponName is Name, or whose class name ends with it
	 * (ShooterWeapon_AK47: "ak47"), any case; null when none matches. The weapon classes are listed once, at the first
	 * call (the buy menu asks every frame).
	 */
	[[nodiscard]] static UClass* FindWeaponClass(const FString& Name);
	/** Every concrete weapon class, in the order FindWeaponClass lists them (the tests' table). */
	static void GetWeaponClasses(TArray<UClass*>& OutClasses);

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
	/** The weapon's mesh asset (the world model), if loaded. */
	[[nodiscard]] UStaticMesh* GetWeaponMesh() const;
	/** The loaded ArmsIdleName and AimOffsetName (null without them). */
	[[nodiscard]] const UBlendSpaceBase* GetArmsIdle() const
	{
		return ArmsIdle;
	}
	[[nodiscard]] const UAimOffsetBlendSpace1D* GetAimOffset() const
	{
		return AimOffset;
	}

	/** Attaches the meshes to the owner and shows the equipped one (UE ShooterGame: AttachMeshToPawn); the pawn calls
	 * it again when its arms or body change. */
	void AttachMeshToPawn();

	/** A notify of the pawn's animations (AShooterCharacter forwards them): MagOut / MagIn play their sounds. */
	virtual void OnAnimNotify(FName NotifyName);

	/**
	 * Plays Animation on the pawn (UE ShooterGame: PlayWeaponAnimation); the longer montage's length, 0 for none. With
	 * a Duration, the montages play at the rate that makes the longer one last Duration (and Duration is returned).
	 */
	float PlayWeaponAnimation(const FShooterWeaponAnim& Animation, float Duration = 0.0f);
	/** Stops it (UE ShooterGame: StopWeaponAnimation). */
	void StopWeaponAnimation(const FShooterWeaponAnim& Animation);

	void PostInitializeComponents() override;
	void Tick(float DeltaSeconds) override;
	/** A weapon on the floor leaves the game mode's pickups. */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
	/** Hides and detaches the meshes (UE ShooterGame: DetachMeshFromPawn). */
	void DetachMeshFromPawn();
	/** Plays a sound at the weapon (UE ShooterGame: PlayWeaponSound). */
	void PlayWeaponSound(USoundWave* Sound, float VolumeMultiplier = 1.0f) const;
	/** The shot's sound's volume now (1; silenced, less). */
	[[nodiscard]] virtual float GetFireVolume() const
	{
		return 1.0f;
	}
	/** A shot's buzz on the owner's pad, when a player at this machine holds it (ps2-shipping N24). */
	void PlayFireForceFeedback() const;
	/** On the floor: the first live pawn near enough with the slot free takes the weapon. */
	void TickPickup();
	/** The world's time (UWorld::GetTimeSeconds). */
	[[nodiscard]] float GetWorldTime() const;

	/** UE ShooterGame: MyPawn. */
	UPROPERTY(Transient)
	AShooterCharacter* MyPawn = nullptr;

	UPROPERTY()
	UStaticMeshComponent* Mesh1P = nullptr;

	UPROPERTY()
	UStaticMeshComponent* Mesh3P = nullptr;

	UPROPERTY(Transient)
	UBlendSpaceBase* ArmsIdle = nullptr;

	UPROPERTY(Transient)
	UAimOffsetBlendSpace1D* AimOffset = nullptr;

	/** The loaded sounds (null when the asset is missing: silent). */
	UPROPERTY(Transient)
	USoundWave* FireSound = nullptr;

	UPROPERTY(Transient)
	USoundWave* ReloadSound = nullptr;

	UPROPERTY(Transient)
	USoundWave* EmptySound = nullptr;

	UPROPERTY(Transient)
	USoundWave* EquipSound = nullptr;

	UPROPERTY(Transient)
	USoundWave* MagOutSound = nullptr;

	UPROPERTY(Transient)
	USoundWave* MagInSound = nullptr;

	EShooterWeaponState CurrentState = EShooterWeaponState::Idle;
	int32 CurrentAmmo = 0;
	int32 CurrentAmmoInClip = 0;
	int32 ShotsFired = 0;
	bool bIsEquipped = false;
	/** On the floor (OnDropped), and from when it can be picked up. */
	bool bDropped = false;
	float PickupTime = 0.0f;
	bool bWantsToFire = false;
	/** A semi-automatic weapon fired for this press. */
	bool bFiredThisPress = false;
	/** The world time of the last shot. */
	float LastFireTime = -1.0e6f;
	/** The equip's and the reload's timers (UE ShooterGame: TimerHandle_OnEquipFinished, TimerHandle_ReloadWeapon). */
	FTimerHandle TimerHandle_OnEquipFinished;
	FTimerHandle TimerHandle_ReloadWeapon;
	/** The weapon is busy (Equipping: no shot, no reload) for Seconds: a draw, a silencer going on or off. */
	void SetEquippingFor(float Seconds);
	/** The weapon is out (its timer). */
	void OnEquipFinished();
	/** The reload is done: the clip fills (its timer). */
	void OnReloadFinished();
};
