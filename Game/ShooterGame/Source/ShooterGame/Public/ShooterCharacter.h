#pragma once

#include "Animation/CharacterAnimInstance.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ShooterTypes.h"
#include "UObject/SoftObjectPath.h"
#include "ShooterCharacter.generated.h"

class AShooterBomb;
class AShooterGameMode;
class AShooterGameState;
class AShooterWeapon;
class UAimOffsetBlendSpace1D;
class UAnimMontage;
class UAnimSequenceBase;
class UBlendSpaceBase;
class UCameraComponent;
class UInputComponent;
class USkeletalMesh;
class USoundWave;
class UShooterCharacterMovement;
struct FDamageEvent;

/**
 * ShooterGame's player and bot pawn (UE ShooterGame: AShooterCharacter; UE's FPS template for the camera): a
 * first-person camera at the eyes that follows the control rotation, Counter-Strike's movement
 * (UShooterCharacterMovement), crouching, walking and jumping, a team-coloured body the other players see, health and
 * armor, and an inventory of weapons.
 *
 * Sizes at CS's 1 unit = 2.54 cm: the capsule is 32 x 72 units (radius 40 cm, half height 91.5 cm), crouched 36 units
 * (half height 46 cm); the eyes are 64 units above the feet (163 cm), 30 crouched (76 cm). The field of view is CS's 90
 * degrees wide at 4:3, 74 degrees high (Leon's cameras keep a vertical field of view).
 *
 * Damage (TakeDamage, UE ShooterGame's order: the actor's TakeDamage first, then the game's rules):
 * - The game mode may refuse it (AShooterGameMode::CanDealDamage: friendly fire).
 * - A point hit (a bullet, a knife) is scaled by its hit group (GetHitGroup, Counter-Strike's): the head by the
 *   weapon's HeadshotMultiplier (4), the stomach by StomachDamageMultiplier (1.25), the legs by LegDamageMultiplier
 *   (0.75), the chest and the arms by 1. Other damage (a blast, the world) is the generic group.
 * - Armor (Counter-Strike): with armor, on every group but the legs (the head only with a helmet), the victim's health
 *   takes ArmorRatio / 2 of the damage (at most all of it) and its armor half of the rest; when the armor runs out the
 *   rest goes to health. ArmorRatio is the weapon's or the projectile's (AShooterWeapon::ArmorRatio); damage from
 *   anything else (the world, a pain volume) ignores armor.
 * - A player's controller hears where the damage came from (AShooterPlayerController::NotifyTakeDamage: the shooter, or
 *   the grenade or the bomb that blew up), for the HUD's damage direction indicator.
 * - A shot that hurts without killing tags the victim (UShooterCharacterMovement::ApplyTagging: CS's velocity
 *   modifier, half the speed, back in a second).
 * - A flashbang whitens the player's view (Flash: the HUD's white holds, then fades) and blinds a bot (IsBlind: its
 *   sensing sees nobody) for part of the fade.
 * - A hard landing hurts (Landed: UShooterCharacterMovement::GetFallDamage, CS's fall damage): the world's damage, with
 *   no instigator or causer, so armor does not take it, nothing tags and a death is the world's in the kill feed.
 * - At 0 health the character dies (Die): the game mode hears of the kill, the pawn drops its best weapon (the primary,
 *   else the pistol), loses the rest, stops colliding and falls (its death montage); a player starts spectating with
 *   the death cam on its killer (AShooterPlayerController::StartDeathCam), a bot lets the pawn go. The corpse stays
 *   CorpseLifeSpan seconds (0: until the round restarts).
 *
 * Inventory (UE ShooterGame: AddWeapon, RemoveWeapon, EquipWeapon, SpawnDefaultInventory): one weapon per slot
 * (EShooterWeaponSlot). The pawn spawns DefaultWeapons at BeginPlay (weapon names, AShooterWeapon::FindWeaponClass:
 * the knife) and its team's (DefaultWeaponsCT: the USP, DefaultWeaponsT: the Glock; a pawn without a team the CT's)
 * when a controller first takes it, each with DefaultWeaponClips clips in the reserve (CS: 12/24, 20/40), and draws
 * the best (primary, secondary, grenade, then the knife). A weapon on the floor is picked up by walking over it when
 * its slot is free (AShooterWeapon's pickup, PickUpWeapon). The grenade slot holds one weapon of each grenade (the HE,
 * the flashbang, the smoke grenade), and its key cycles them (SelectSlot).
 *
 * The bomb (AShooterBomb): the terrorist carrying it plants it by holding Use (E) standing still in a bomb site for
 * the bomb's PlantDuration; a counter-terrorist defuses a planted bomb by holding Use near it (quicker with a defuse
 * kit). Neither moves while planting or defusing (CS). The carrier draws the bomb (5; D-pad down) and drops it with the
 * drop key (G; D-pad right), as CS's C4 in slot 5; a terrorist walking over it takes it. The freeze time at a round's
 * start holds every pawn still and its weapons silent; it can still look around.
 *
 * Animation (Docs/PLANS/ps2-shipping.md N25, N27): the team's skinned body (CTBodyMeshName, TBodyMeshName, on
 * ACharacter's skeletal mesh) plays a UCharacterAnimInstance: the locomotion blend space by speed and direction
 * (LocomotionBlendSpaceName; CrouchBlendSpaceName while crouched), the jump states (JumpStartAnimName,
 * JumpLoopAnimName, JumpLandAnimName), the upper body's montages from UpperBodyBranchBone and the aim offset by the
 * view's pitch (the drawn weapon's stance, AShooterWeapon::AimOffsetName, else AimOffsetName; none while planting,
 * defusing or dead). The team's first-person arms (CTArmsMeshName, TArmsMeshName: Mesh1P, a skinned view model only its
 * player sees) idle with the drawn weapon's pose (AShooterWeapon::ArmsIdleName). The drawn weapon sits on
 * WeaponSocketName of the arms and of the body (AShooterWeapon::AttachMeshToPawn); the weapons' fire, reload and draw
 * montages and the bomb's plant and defuse ones play on both (PlayPawnMontages), and a death montage (DeathAnimBackName
 * when shot from the front, DeathAnimFrontName from behind) lays the body down and holds it. The animations' notifies
 * come back to OnBodyAnimNotify / OnArmsAnimNotify: footsteps (below), and the rest go to the drawn weapon (its
 * magazine), from the arms in first person and from the body otherwise. The body evaluates its pose only when drawn,
 * and less often far from the view; the arms only when drawn (USkeletalMeshComponent's throttling).
 *
 * Footsteps (CS): the body's locomotion notifies (Footstep_L, Footstep_R) are the steps. A step on the floor faster
 * than CS's 150 units a second (381 cm/s) plays the floor's sound and makes a noise the bots hear (AActor::MakeNoise at
 * FootstepNoiseLoudness); slower ones are silent, so walking (the walk key, 330 cm/s) and crouching (212 cm/s) make
 * none. Without a skinned body there are no footsteps (no notifies). The floor is what a line down from the feet hits
 * (its physical material's surface, GetFloorSurface: CS's texture under the player): FootstepSounds has each
 * surface's left and right step (CS's pl_step, pl_dirt, pl_tile, pl_metal...). On a ladder (N30c) a climb faster than
 * that plays LadderStepSoundNames' sounds in turn every LadderStepInterval (CS's pl_ladder) with the same noise.
 *
 * Input (Config/DefaultInput.ini): MoveForward / MoveRight (W A S D; the left stick), Turn / LookUp (the mouse),
 * TurnRate / LookUpRate (the right stick: BaseTurnRate / BaseLookUpRate degrees a second at full tilt), Jump (Space;
 * Cross), Crouch (Left Ctrl; Circle: a press toggles, or held with the player's option), Walk (Left Shift: held; L3),
 * Fire (the left button; R2), Targeting (the right button: the AWP's zoom, a silencer, the Glock's burst, the knife's
 * stab; L2), Reload (R; Square), PrimaryWeapon / SecondaryWeapon / Knife / Grenade / Bomb (1, 2, 3, 4, 5; R1, L1,
 * D-pad up, D-pad left, D-pad down), DropWeapon (G; D-pad right: the weapon in hand, or the bomb when drawn), Use (E:
 * held; Triangle). A dead pawn ignores them, and the buy menu takes its keys while it is open
 * (AShooterPlayerController).
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AShooterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The eyes: CS's view height above the feet (64 units standing), cm. */
	static constexpr float StandingEyeHeight = 163.0f;
	/** The eyes while crouched (30 units), cm. */
	static constexpr float CrouchingEyeHeight = 76.0f;

	void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	void Tick(float DeltaSeconds) override;
	/** Full health (UE ShooterGame: PostInitializeComponents). */
	void PostInitializeComponents() override;
	/**
	 * Joins the game mode's pawns (AShooterGameMode::RegisterPawn) and spawns the default inventory (UE ShooterGame:
	 * SpawnDefaultInventory, in PostInitializeComponents there).
	 */
	void BeginPlay() override;
	/** Leaves the game mode's pawns; the inventory goes with the pawn (UE ShooterGame: DestroyInventory in Destroyed).
	 */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Takes the team's body when a controller with a player state takes the pawn. */
	void PossessedBy(AController* NewController) override;
	/** The game mode counts its live pawns again (a pawn changed hands). */
	void UnPossessed() override;
	/** The rules of the class comment; returns the health taken. */
	float TakeDamage(
		float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	/** The movement's timers (the jump stamina, the tagging) run down, then the move (UShooterCharacterMovement). */
	void PerformMovement(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw = nullptr) override;
	/** On a ladder a jump pushes off it (UShooterCharacterMovement::JumpOffLadder); else ACharacter's jump. */
	void Jump() override;
	/** A hard landing's fall damage (the class comment). */
	void Landed(const FHitResult& Hit) override;
	/** The jump costs its stamina (UShooterCharacterMovement::StartJumpStamina). */
	void OnJumped() override;

	/**
	 * What the floor under the feet is made of: the physical material's surface of what a line from the feet 50 cm down
	 * meets (the pawn left out; CS: the texture under the player), SHOOTER_SURFACE_Default when it meets nothing.
	 */
	[[nodiscard]] EPhysicalSurface GetFloorSurface() const;
	/** The last step's sound (a footstep or a ladder's; null before any or without sounds) and floor (the tests). */
	[[nodiscard]] USoundWave* GetLastFootstepSound() const
	{
		return LastFootstepSound;
	}
	[[nodiscard]] EPhysicalSurface GetLastFootstepSurface() const
	{
		return LastFootstepSurface.GetValue();
	}

	/** The first-person camera (UE FPS template: FirstPersonCameraComponent). */
	[[nodiscard]] UCameraComponent* GetFirstPersonCameraComponent() const
	{
		return FirstPersonCameraComponent;
	}
	/** The first-person arms (UE ShooterGame: Mesh1P): a skinned view model on the camera, seen by its owner only. */
	[[nodiscard]] USkeletalMeshComponent* GetMesh1P() const
	{
		return Mesh1P;
	}
	/** The body has a mesh (ACharacter::GetMesh(), the body the other players see: bOwnerNoSee). */
	[[nodiscard]] bool HasSkeletalBody() const;
	/** The arms have a mesh. */
	[[nodiscard]] bool HasArms() const;

	/**
	 * Shows InMesh as the body (null: none; the weapon then sits on the capsule at its ThirdPersonOffset): ACharacter's
	 * mesh takes it with a UCharacterAnimInstance set up from the config (locomotion, crouch, jump, aim offset, upper
	 * body branch), and the drawn weapon moves to its hand socket. UpdateBody calls it with the team's mesh; the tests
	 * with theirs.
	 */
	void SetSkeletalBody(USkeletalMesh* InMesh);
	/** Shows InMesh as the first-person arms (null: none; the weapon then sits at its FirstPersonOffset). */
	void SetArmsMesh(USkeletalMesh* InMesh);
	/** The arms' base pose while a weapon is drawn: its idle blend space (null: the reference pose). */
	void SetArmsIdle(const UBlendSpaceBase* Idle);

	/**
	 * Plays an animation on the pawn (UE ShooterGame: PlayWeaponAnimation): Pawn1P on the arms and Pawn3P on the body,
	 * each where the mesh is there. Returns the longer one's length at PlayRate, 0 when none played.
	 */
	float PlayPawnMontages(const FShooterWeaponAnim& Animation, float PlayRate = 1.0f);
	/** Stops them (UE ShooterGame: StopWeaponAnimation). */
	void StopPawnMontages(const FShooterWeaponAnim& Animation);

	/**
	 * Where the drawn weapon's first-person mesh goes: the arms and WeaponSocketName when the pawn has arms, else the
	 * camera (OutSocketName NAME_None: the weapon's FirstPersonOffset places it).
	 */
	[[nodiscard]] USceneComponent* GetWeaponAttachParent1P(FName& OutSocketName) const;
	/** The same for the body's weapon: the skinned body's socket, else the static body (ThirdPersonOffset). */
	[[nodiscard]] USceneComponent* GetWeaponAttachParent3P(FName& OutSocketName) const;
	/** The movement, as the game's class. */
	[[nodiscard]] UShooterCharacterMovement* GetShooterCharacterMovement() const;

	/** The team of the controller's player state, None without one (a dead pawn keeps the team it died in). */
	[[nodiscard]] EShooterTeam GetTeam() const;

	/** The camera's vertical field of view without a zoom (CS's 90 degrees wide at 4:3). */
	[[nodiscard]] static float GetDefaultFieldOfView();

	/** A local player views the game through this pawn (UE ShooterGame: IsFirstPerson). */
	[[nodiscard]] bool IsFirstPerson() const;

	/** Holds the walk key (the walk speed until released). */
	void SetWalking(bool bNewWalking);

	/** The axes (UE FPS template: MoveForward / MoveRight along the view's yaw). */
	void MoveForward(float Value);
	void MoveRight(float Value);
	/** A stick's rate, -1..1, turned into degrees this frame (UE templates: TurnAtRate / LookUpAtRate). */
	void TurnAtRate(float Rate);
	void LookUpAtRate(float Rate);
	/** The scale of the stick's rates: the controlling player's aim sensitivity (UShooterPersistentUser), else 1. */
	[[nodiscard]] float GetAimSensitivity() const;

	// Health and armor

	/** UE ShooterGame: IsAlive. */
	[[nodiscard]] bool IsAlive() const
	{
		return Health > 0.0f;
	}
	[[nodiscard]] float GetHealth() const
	{
		return Health;
	}
	[[nodiscard]] float GetMaxHealth() const
	{
		return MaxHealth;
	}
	/** The armor points (CS: kevlar, 0 to 100). */
	[[nodiscard]] float GetArmor() const
	{
		return Armor;
	}
	[[nodiscard]] bool HasHelmet() const
	{
		return bHasHelmet;
	}
	/** Sets the armor (clamped to MaxArmor) and the helmet (buying kevlar). */
	void SetArmor(float NewArmor, bool bNewHasHelmet);
	/** Full health, no armor (a new pawn). */
	void ResetHealth();
	/** Takes no damage while on (the `god` command). */
	void SetGodMode(bool bEnabled)
	{
		bGodMode = bEnabled;
	}
	[[nodiscard]] bool IsGodMode() const
	{
		return bGodMode;
	}

	/**
	 * Where a hit at Location struck (Counter-Strike's hit groups on height bands of the capsule, which stands on its
	 * feet): the head is the top HeadHeight cm; below it the body, its bottom LegsFraction the legs, up to
	 * StomachFraction the stomach, the rest the chest, or an arm where the hit is farther than ArmFraction of the
	 * radius to the pawn's side. Left and right are the pawn's.
	 */
	[[nodiscard]] EShooterHitGroup GetHitGroup(const FVector& Location) const;
	/** A group's damage multiplier (the head's is the weapon's HeadshotMultiplier, HeadMultiplier here). */
	[[nodiscard]] float GetHitGroupMultiplier(EShooterHitGroup HitGroup, float HeadMultiplier) const;

	/**
	 * The health and armor a hit of Damage on HitGroup takes with Armor points, ArmorRatio and a helmet, by the class
	 * comment's rule (shared with the tests).
	 */
	static void ComputeArmorDamage(float Damage, float ArmorRatio, EShooterHitGroup HitGroup, bool bHelmet, float Armor,
		float& OutHealthDamage, float& OutArmorDamage);

	/** Kills the pawn at once (`kill`, bot_kick); the killer is the pawn's own controller. */
	void Suicide();

	// Flashbangs

	/**
	 * A flashbang went off in view (AShooterProjectile_Flashbang): the view holds Alpha white for HoldTime seconds and
	 * fades out in FadeTime; the pawn is blind for BlindTime (a bot sees nobody). A weaker flash while a stronger one
	 * lasts changes nothing.
	 */
	void Flash(float HoldTime, float FadeTime, float Alpha, float BlindTime);
	/** How white the view is now (0 to 1: the HUD's full-screen white). */
	[[nodiscard]] float GetFlashAlpha() const;
	/** The last flash's hold and fade, seconds. */
	[[nodiscard]] float GetFlashHoldTime() const
	{
		return FlashHoldTime;
	}
	[[nodiscard]] float GetFlashFadeTime() const
	{
		return FlashFadeTime;
	}
	/** Blinded by a flashbang now. */
	[[nodiscard]] bool IsBlind() const;
	/** The flash is over (a new round). */
	void ClearFlash();

	/**
	 * A survivor's new round (CS): full health, the armor and the weapons kept, standing still at Feet facing Yaw, the
	 * trigger, the zoom, a plant or a defuse stopped.
	 */
	void ResetForNewRound(const FVector& Feet, float Yaw);

	/** The round's freeze time holds the pawn (AShooterGameState::IsFreezeTime), or the match is over. */
	[[nodiscard]] bool IsFrozen() const;
	/** The game mode's round state (null outside a ShooterGame match: tests, other game modes). */
	[[nodiscard]] const AShooterGameState* GetShooterGameState() const;
	/** The world's game mode, as the game's class (null in a world without one). */
	[[nodiscard]] AShooterGameMode* GetShooterGameMode() const;

	// The bomb

	/** The bomb this pawn carries (AShooterBomb::GiveTo / Drop / Plant set it). */
	[[nodiscard]] AShooterBomb* GetCarriedBomb() const
	{
		return CarriedBomb;
	}
	/** Sets the bomb carried; when it goes (dropped, planted, destroyed) while drawn, the best weapon is drawn. */
	void SetCarriedBomb(AShooterBomb* Bomb);
	/**
	 * Draws the carried bomb (CS 1.6's slot 5): the weapon in hand is put away and the arms hidden (the C4 has no
	 * first-person model). Drawing a weapon puts the bomb away. False without a bomb, dead or already drawn.
	 */
	bool DrawBomb();
	/**
	 * The Fire key (CS): with the bomb drawn it plants while held, as Use does (StartUse / StopUse); otherwise the
	 * weapon fires (StartWeaponFire / StopWeaponFire).
	 */
	void OnFirePressed();
	void OnFireReleased();
	/** The bomb is drawn (DrawBomb). */
	[[nodiscard]] bool IsBombDrawn() const
	{
		return bBombDrawn;
	}
	/**
	 * Drops the carried bomb ahead of the feet, as a weapon falls (CS's drop key with the C4 drawn); the dropper cannot
	 * take it back for the bomb's PickupDelay. False without a bomb, dead or planting.
	 */
	bool DropBomb();
	/** A counter-terrorist's defuse kit (the buy menu; lost with the pawn). */
	[[nodiscard]] bool HasDefuseKit() const
	{
		return bHasDefuseKit;
	}
	void SetDefuseKit(bool bNewHasKit)
	{
		bHasDefuseKit = bNewHasKit;
	}
	/**
	 * The use key (E) pressed: a carrier in a bomb site starts planting, a counter-terrorist near the planted bomb
	 * starts defusing. True when either started.
	 */
	bool StartUse();
	/** The use key released: the plant or the defuse stops. */
	void StopUse();
	[[nodiscard]] bool IsPlanting() const
	{
		return bIsPlanting;
	}
	[[nodiscard]] bool IsDefusing() const
	{
		return DefusingBomb != nullptr;
	}
	/** The world time a plant in progress ends. */
	[[nodiscard]] float GetPlantEndTime() const;
	/** The bomb site (its second tag, "A" or "B") the pawn stands in, NAME_None outside. */
	[[nodiscard]] FName GetBombSiteHere() const;

	// Inventory

	/** Adds a spawned weapon to its slot: a weapon already there is dropped. Draws it when nothing is drawn. */
	void AddWeapon(AShooterWeapon* Weapon);
	/** Takes a weapon out of the inventory (it is not destroyed); draws the best one left when it was drawn. */
	void RemoveWeapon(AShooterWeapon* Weapon);
	/** Spawns a weapon of WeaponClass and adds it; returns it (null for a class that is not a weapon). */
	AShooterWeapon* GiveWeapon(UClass* WeaponClass);
	/** Draws a weapon of the inventory (UE ShooterGame: EquipWeapon). */
	void EquipWeapon(AShooterWeapon* Weapon);
	/**
	 * Draws the weapon of a slot, if any; the grenade slot's next grenade (by GrenadeOrder, wrapping) when a grenade
	 * is drawn (CS: the grenade key cycles the HE, the flashbang and the smoke grenade).
	 */
	void SelectSlot(EShooterWeaponSlot Slot);
	/** The inventory's weapon of WeaponClass, or null. */
	[[nodiscard]] AShooterWeapon* FindWeaponOfClass(const UClass* WeaponClass) const;
	/**
	 * Draws the best weapon with ammunition (AShooterWeapon::HasAmmo; one that can reload counts): primary, secondary,
	 * grenade (unless bWithGrenades is false: a bot in a fight, whose throws are its own), knife. When none has any,
	 * the weapon in hand stays (or, with none in hand, the first by that order).
	 */
	void EquipBestWeapon(bool bWithGrenades = true);
	/** Drops a weapon of the inventory at the pawn's feet, ahead; returns true when dropped. */
	bool DropWeapon(AShooterWeapon* Weapon);
	/**
	 * Takes a weapon from the floor (AShooterWeapon's pickup: walking over it with its slot free): adds it, plays its
	 * draw sound when it is not drawn at once (drawn, its draw plays it) and tells the player (NotifyPickup).
	 */
	void PickUpWeapon(AShooterWeapon* Weapon);
	/** Tells the controlling player what was picked up (the HUD's "Picked up <item>"); a bot hears nothing. */
	void NotifyPickup(const FString& ItemName) const;
	/** Destroys every weapon of the inventory (UE ShooterGame: DestroyInventory). */
	void DestroyInventory();
	/** The drawn weapon (UE ShooterGame: GetWeapon). */
	[[nodiscard]] AShooterWeapon* GetWeapon() const
	{
		return CurrentWeapon;
	}
	/** The weapon in a slot, or null. */
	[[nodiscard]] AShooterWeapon* GetWeaponInSlot(EShooterWeaponSlot Slot) const;
	[[nodiscard]] const TArray<AShooterWeapon*>& GetInventory() const
	{
		return Inventory;
	}

	/** The trigger and the other weapon buttons (UE ShooterGame: StartWeaponFire / StopWeaponFire). */
	void StartWeaponFire();
	void StopWeaponFire();
	void StartSecondaryFire();
	void ReloadWeapon();

	/** How fast the camera follows the eyes' height when crouching or standing (FMath::FInterpTo speed). */
	UPROPERTY(Config)
	float EyeHeightInterpSpeed = 14.0f;

	/** The teams' skinned bodies (SK_ of /Game/Characters, made in Blender: SourceArt/Characters); empty: none. */
	UPROPERTY(Config)
	FSoftObjectPath CTBodyMeshName;

	UPROPERTY(Config)
	FSoftObjectPath TBodyMeshName;

	/** The teams' first-person arms (SK_ of /Game/Characters/Arms); empty: none. */
	UPROPERTY(Config)
	FSoftObjectPath CTArmsMeshName;

	UPROPERTY(Config)
	FSoftObjectPath TArmsMeshName;

	/**
	 * The body's anim graph: the locomotion blend space (speed in cm/s by direction in degrees) standing and crouched,
	 * the default aim offset (a weapon's stance replaces it).
	 */
	UPROPERTY(Config)
	FSoftObjectPath LocomotionBlendSpaceName;

	UPROPERTY(Config)
	FSoftObjectPath CrouchBlendSpaceName;

	UPROPERTY(Config)
	FSoftObjectPath AimOffsetName;

	/** The jump's clips (UCharacterAnimInstance's jump states): the take-off, the fall's loop, the landing. */
	UPROPERTY(Config)
	FSoftObjectPath JumpStartAnimName;

	UPROPERTY(Config)
	FSoftObjectPath JumpLoopAnimName;

	UPROPERTY(Config)
	FSoftObjectPath JumpLandAnimName;

	/** The death montages (the whole body, a last section looping on the pose lying down): shot from the front, the
	 * body falls on its back; from behind, on its front. */
	UPROPERTY(Config)
	FSoftObjectPath DeathAnimBackName;

	UPROPERTY(Config)
	FSoftObjectPath DeathAnimFrontName;

	/** The bomb's montages of the arms (1P) and the body (3P). */
	UPROPERTY(Config)
	FSoftObjectPath PlantAnim1PName;

	UPROPERTY(Config)
	FSoftObjectPath PlantAnim3PName;

	UPROPERTY(Config)
	FSoftObjectPath DefuseAnim1PName;

	UPROPERTY(Config)
	FSoftObjectPath DefuseAnim3PName;

	/** Each surface's footsteps: the left foot's (Footstep_L) the first, the right foot's the second. */
	UPROPERTY(Config)
	FShooterSurfaceSounds FootstepSounds;

	/** The steps on a ladder, played in turn (CS: pl_ladder). */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> LadderStepSoundNames;

	/** Seconds between two steps on a ladder (CS: 0.35 s). */
	UPROPERTY(Config)
	float LadderStepInterval = 0.35f;

	/** How far the bots hear a footstep (AActor::MakeNoise's loudness; a shot is 1). */
	UPROPERTY(Config)
	float FootstepNoiseLoudness = 0.5f;

	/** The socket of the hand the weapon sits on, on the arms and on the body. */
	UPROPERTY(Config)
	FName WeaponSocketName = TEXT("Weapon_R");

	/** The bone the upper body starts at: the montages of the UpperBody slot and the aim offset move it and above. */
	UPROPERTY(Config)
	FName UpperBodyBranchBone = TEXT("spine_01");

	/** Health at spawn (CS: 100). */
	UPROPERTY(Config)
	float MaxHealth = 100.0f;

	/** The most armor points (CS: 100). */
	UPROPERTY(Config)
	float MaxArmor = 100.0f;

	/** The top of the capsule that counts as the head, cm (CS's head hit box is about 11 units tall). */
	UPROPERTY(Config)
	float HeadHeight = 28.0f;

	/**
	 * The body's bands below the head, fractions of its height from the feet: the legs up to LegsFraction, the stomach
	 * up to StomachFraction, the chest above (CS's hit boxes: the legs to the hips, 36 of 61 units). An arm is a chest
	 * hit farther than ArmFraction of the capsule's radius to the pawn's side.
	 */
	UPROPERTY(Config)
	float LegsFraction = 0.59f;

	UPROPERTY(Config)
	float StomachFraction = 0.75f;

	UPROPERTY(Config)
	float ArmFraction = 0.6f;

	/** The hit groups' damage multipliers (CS: the stomach 1.25, the legs 0.75; the chest and the arms 1). */
	UPROPERTY(Config)
	float StomachDamageMultiplier = 1.25f;

	UPROPERTY(Config)
	float LegDamageMultiplier = 0.75f;

	/** The weapons every pawn spawns with, by name (CS: the knife; `+DefaultWeapons=knife`). */
	UPROPERTY(Config)
	TArray<FString> DefaultWeapons;

	/** The team's weapons, given when a controller first takes the pawn (CS: the USP, the Glock). */
	UPROPERTY(Config)
	TArray<FString> DefaultWeaponsCT;

	UPROPERTY(Config)
	TArray<FString> DefaultWeaponsT;

	/** The clips of ammunition a default weapon brings in its reserve (CS: two). */
	UPROPERTY(Config)
	int32 DefaultWeaponClips = 2;

	/** Seconds a corpse stays; 0 until the round restarts removes it. */
	UPROPERTY(Config)
	float CorpseLifeSpan = 0.0f;

	/** Degrees a second the view turns with the right stick at full tilt (UE templates: BaseTurnRate). */
	UPROPERTY(Config)
	float BaseTurnRate = 150.0f;

	/** Degrees a second the view pitches with the right stick at full tilt (UE templates: BaseLookUpRate). */
	UPROPERTY(Config)
	float BaseLookUpRate = 100.0f;

private:
	void OnJumpPressed();
	void OnCrouchPressed();
	void OnCrouchReleased();
	void OnWalkPressed();
	void OnWalkReleased();
	void OnSelectPrimary();
	void OnSelectSecondary();
	void OnSelectKnife();
	void OnSelectGrenade();
	void OnSelectBomb();
	void OnDropWeapon();
	void OnUsePressed();
	void OnUseReleased();
	/** The plant in progress: cancelled when the conditions go, finished by its timer (OnPlantTimer). */
	void TickPlanting();
	/** Whether the pawn may plant now: alive, carrying, in a site, on the floor, still. */
	[[nodiscard]] bool CanPlant() const;

	/** The crouch key toggles (the controlling player's option, UShooterPersistentUser::bToggleCrouch). */
	[[nodiscard]] bool IsCrouchToggle() const;
	/** Where a dropped item lands: ahead of the feet, short of a wall, on the floor below (Dropped ignored). */
	[[nodiscard]] FVector GetDropLocation(const AActor* Dropped) const;
	/** The drawn bomb goes back (a weapon is drawn, the bomb left): the arms show again. */
	void PutAwayBomb();

	/** Sets the body's mesh for the team. */
	void UpdateBody();
	/** The body's anim graph's inputs from the movement and the view: speed, direction, aim pitch, falling. */
	void UpdateBodyAnimation();
	/** The drawn weapon moves to the arms' or the body's socket (they changed). */
	void ReattachWeapon();
	/** The notifies of the body's and the arms' animations (bound to their anim instances' OnAnimNotify). */
	void OnBodyAnimNotify(FName NotifyName, const UAnimSequenceBase* Animation);
	void OnArmsAnimNotify(FName NotifyName, const UAnimSequenceBase* Animation);
	/** A footstep notify: the sound and the noise when the step is heard (the class comment). */
	void PlayFootstep(bool bLeftFoot);
	/** A step on a ladder every LadderStepInterval while climbing (the class comment). */
	void TickLadderSteps(float DeltaSeconds);
	/** Spawns DefaultWeapons and draws the best (UE ShooterGame: SpawnDefaultInventory). */
	void SpawnDefaultInventory();
	/** Spawns the team's default weapons (once, when a controller first takes the pawn) and draws the best. */
	void SpawnTeamInventory();
	/** Gives the weapons Names name, each with DefaultWeaponClips clips in its reserve. */
	void GiveDefaultWeapons(const TArray<FString>& Names);
	/**
	 * Death (UE ShooterGame: OnDeath): see the class comment. Killer is the controller credited with the kill (the
	 * pawn's own for a suicide or the world).
	 */
	void Die(AController* Killer, AActor* DamageCauser, bool bHeadshot);

	/** UE FPS template: FirstPersonCameraComponent (bUsePawnControlRotation). */
	UPROPERTY()
	UCameraComponent* FirstPersonCameraComponent = nullptr;

	/** UE ShooterGame: Mesh1P, the first-person arms. */
	UPROPERTY()
	USkeletalMeshComponent* Mesh1P = nullptr;

	/** The loaded animation assets (null where the config names none). */
	UPROPERTY(Transient)
	UBlendSpaceBase* LocomotionBlendSpace = nullptr;

	UPROPERTY(Transient)
	UBlendSpaceBase* CrouchBlendSpace = nullptr;

	UPROPERTY(Transient)
	UAimOffsetBlendSpace1D* AimOffset = nullptr;

	UPROPERTY(Transient)
	FAnimJumpClips JumpClips;

	UPROPERTY(Transient)
	UAnimMontage* DeathAnimBack = nullptr;

	UPROPERTY(Transient)
	UAnimMontage* DeathAnimFront = nullptr;

	UPROPERTY(Transient)
	FShooterWeaponAnim PlantAnim;

	UPROPERTY(Transient)
	FShooterWeaponAnim DefuseAnim;

	/** FootstepSounds and LadderStepSoundNames, loaded. */
	UPROPERTY(Transient)
	FShooterSurfaceSoundSet FootstepSoundSet;

	UPROPERTY(Transient)
	TArray<USoundWave*> LadderStepSounds;

	/** The last step's sound and floor (the tests). */
	UPROPERTY(Transient)
	USoundWave* LastFootstepSound = nullptr;

	TEnumAsByte<EPhysicalSurface> LastFootstepSurface = SurfaceType_Default;
	/** Climbing time toward the next ladder step, and the ladder steps so far (the next one's sound). */
	float LadderStepTime = 0.0f;
	int32 NumLadderSteps = 0;

	/** One weapon a slot (UE ShooterGame: Inventory). */
	UPROPERTY(Transient)
	TArray<AShooterWeapon*> Inventory;

	/** The drawn weapon (UE ShooterGame: CurrentWeapon). */
	UPROPERTY(Transient)
	AShooterWeapon* CurrentWeapon = nullptr;

	UPROPERTY(Transient)
	float Health = 0.0f;

	UPROPERTY(Transient)
	float Armor = 0.0f;

	UPROPERTY(Transient)
	bool bHasHelmet = false;

	/** The bomb carried, and the one being defused. */
	UPROPERTY(Transient)
	AShooterBomb* CarriedBomb = nullptr;

	UPROPERTY(Transient)
	AShooterBomb* DefusingBomb = nullptr;

	bool bHasDefuseKit = false;
	bool bIsPlanting = false;
	/** The carried bomb is drawn (DrawBomb): no weapon in hand. */
	bool bBombDrawn = false;
	/** The plant was started by the Fire key, which stops it when released. */
	bool bPlantingWithFire = false;
	/** The team's default weapons were given (SpawnTeamInventory). */
	bool bTeamInventoryGiven = false;
	/** The last flash: when it began, its hold, fade and white, and when the blindness ends (world time). */
	float FlashStartTime = -1.0e6f;
	float FlashHoldTime = 0.0f;
	float FlashFadeTime = 0.0f;
	float FlashMaxAlpha = 0.0f;
	float BlindEndTime = -1.0e6f;
	/** The plant's timer (OnPlantTimer). */
	FTimerHandle TimerHandle_Plant;
	/** The plant's time is up: the bomb goes down if the planter still can. */
	void OnPlantTimer();
	bool bGodMode = false;
	/** The team at death (the controller and its player state leave the corpse). */
	EShooterTeam DeadTeam = EShooterTeam::None;
};
