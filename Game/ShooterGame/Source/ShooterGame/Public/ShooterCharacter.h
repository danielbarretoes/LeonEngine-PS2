#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ShooterTypes.h"
#include "UObject/SoftObjectPath.h"
#include "ShooterCharacter.generated.h"

class AShooterWeapon;
class UCameraComponent;
class UInputComponent;
class UShooterCharacterMovement;
class UStaticMeshComponent;
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
 * - A point hit in the head (GetHitGroup: the top HeadHeight cm of the capsule) is multiplied by the weapon's
 *   HeadshotMultiplier.
 * - Armor (Counter-Strike): with armor, and on the body or with a helmet, the victim's health takes ArmorRatio / 2 of
 *   the damage (at most all of it) and its armor half of the rest; when the armor runs out the rest goes to health.
 *   ArmorRatio is the weapon's or the projectile's (AShooterWeapon::ArmorRatio); damage from anything else (the
 *   world, a pain volume) ignores armor.
 * - At 0 health the character dies (Die): the game mode hears of the kill, the pawn drops its best weapon (the primary,
 *   else the pistol), loses the rest, stops colliding and lies down; a player starts spectating
 *   (APlayerController::ChangeState(NAME_Spectating)), a bot lets the pawn go. The corpse stays CorpseLifeSpan
 *   seconds (0: until the round restarts).
 *
 * Inventory (UE ShooterGame: AddWeapon, RemoveWeapon, EquipWeapon, SpawnDefaultInventory): one weapon per slot
 * (EShooterWeaponSlot). The pawn spawns DefaultWeapons at BeginPlay (weapon names, AShooterWeapon::FindWeaponClass)
 * and draws the best (primary, then secondary, then grenade). A weapon on the floor is picked up by walking over it
 * when its slot is free (AShooterWeapon's pickup).
 *
 * Input (Config/DefaultInput.ini): MoveForward / MoveRight (W A S D), Turn / LookUp (the mouse), Jump (Space), Crouch
 * (Left Ctrl, C: held), Walk (Left Shift: held), Fire (the left button), Targeting (the right button: a sniper's zoom),
 * Reload (R), PrimaryWeapon / SecondaryWeapon / Grenade (1, 2, 4), DropWeapon (G). A dead pawn ignores them.
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
	/** Spawns the default inventory (UE ShooterGame: SpawnDefaultInventory in PostInitializeComponents). */
	void BeginPlay() override;
	/** The inventory goes with the pawn (UE ShooterGame: DestroyInventory in Destroyed). */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Takes the team's body when a controller with a player state takes the pawn. */
	void PossessedBy(AController* NewController) override;
	/** The rules of the class comment; returns the health taken. */
	float TakeDamage(
		float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** The first-person camera (UE FPS template: FirstPersonCameraComponent). */
	[[nodiscard]] UCameraComponent* GetFirstPersonCameraComponent() const
	{
		return FirstPersonCameraComponent;
	}
	/** The body the other players see (bOwnerNoSee: not in its own player's view). */
	[[nodiscard]] UStaticMeshComponent* GetBodyMesh() const
	{
		return BodyMesh;
	}
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
	/** Sets the armor (clamped to MaxArmor) and the helmet (the buy menu, a round's restart). */
	void SetArmor(float NewArmor, bool bNewHasHelmet);
	/** Back to full health, no armor (a round's restart). */
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

	/** Where a hit at Location struck: the head is the top HeadHeight cm of the capsule. */
	[[nodiscard]] EShooterHitGroup GetHitGroup(const FVector& Location) const;

	/**
	 * The health and armor a hit of Damage takes with Armor points, ArmorRatio and a helmet, by the class comment's
	 * rule (shared with the tests).
	 */
	static void ComputeArmorDamage(float Damage, float ArmorRatio, bool bHeadshot, bool bHelmet, float Armor,
		float& OutHealthDamage, float& OutArmorDamage);

	/** Kills the pawn at once (the round's end, `kill`); the killer is the pawn's own controller. */
	void Suicide();

	// Inventory

	/** Adds a spawned weapon to its slot: a weapon already there is dropped. Draws it when nothing is drawn. */
	void AddWeapon(AShooterWeapon* Weapon);
	/** Takes a weapon out of the inventory (it is not destroyed); draws the best one left when it was drawn. */
	void RemoveWeapon(AShooterWeapon* Weapon);
	/** Spawns a weapon of WeaponClass and adds it; returns it (null for a class that is not a weapon). */
	AShooterWeapon* GiveWeapon(UClass* WeaponClass);
	/** Draws a weapon of the inventory (UE ShooterGame: EquipWeapon). */
	void EquipWeapon(AShooterWeapon* Weapon);
	/** Draws the weapon of a slot, if any. */
	void SelectSlot(EShooterWeaponSlot Slot);
	/** Draws the best weapon: primary, secondary, grenade. */
	void EquipBestWeapon();
	/** Drops a weapon of the inventory at the pawn's feet, ahead; returns true when dropped. */
	bool DropWeapon(AShooterWeapon* Weapon);
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

	/** The bodies of the teams (boxes made in Blender, SourceArt/Characters): meshes of /Game/Characters. */
	UPROPERTY(Config)
	FSoftObjectPath CTBodyMeshName;

	UPROPERTY(Config)
	FSoftObjectPath TBodyMeshName;

	/** Health at spawn (CS: 100). */
	UPROPERTY(Config)
	float MaxHealth = 100.0f;

	/** The most armor points (CS: 100). */
	UPROPERTY(Config)
	float MaxArmor = 100.0f;

	/** The top of the capsule that counts as the head, cm (CS's head hit box is about 11 units tall). */
	UPROPERTY(Config)
	float HeadHeight = 28.0f;

	/** The weapons a pawn spawns with, by name (CS: the pistol; `+DefaultWeapons=usp`). */
	UPROPERTY(Config)
	TArray<FString> DefaultWeapons;

	/** Seconds a corpse stays; 0 until the round restarts removes it. */
	UPROPERTY(Config)
	float CorpseLifeSpan = 0.0f;

private:
	void OnJumpPressed();
	void OnCrouchPressed();
	void OnCrouchReleased();
	void OnWalkPressed();
	void OnWalkReleased();
	void OnSelectPrimary();
	void OnSelectSecondary();
	void OnSelectGrenade();
	void OnDropWeapon();

	/** Sets the body's mesh for the team. */
	void UpdateBody();
	/** Spawns DefaultWeapons and draws the best (UE ShooterGame: SpawnDefaultInventory). */
	void SpawnDefaultInventory();
	/**
	 * Death (UE ShooterGame: OnDeath): see the class comment. Killer is the controller credited with the kill (the
	 * pawn's own for a suicide or the world).
	 */
	void Die(AController* Killer, AActor* DamageCauser, bool bHeadshot);

	/** UE FPS template: FirstPersonCameraComponent (bUsePawnControlRotation). */
	UPROPERTY()
	UCameraComponent* FirstPersonCameraComponent = nullptr;

	/** The team-coloured body, standing on the feet. */
	UPROPERTY()
	UStaticMeshComponent* BodyMesh = nullptr;

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

	bool bGodMode = false;
	/** The team at death (the controller and its player state leave the corpse). */
	EShooterTeam DeadTeam = EShooterTeam::None;
};
