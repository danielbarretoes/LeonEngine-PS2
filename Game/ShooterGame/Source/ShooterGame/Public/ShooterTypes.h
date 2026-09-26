#pragma once

#include "CoreMinimal.h"
#include "ShooterTypes.generated.h"

/**
 * The two sides of a match (Counter-Strike's counter-terrorists and terrorists; UE ShooterGame numbers its teams).
 * None is a player not placed in a team yet.
 */
UENUM()
enum class EShooterTeam : uint8
{
	None,
	CT,
	T,
};

/**
 * The inventory slot a weapon takes (Counter-Strike's slots 1, 2 and 4; the knife's slot 3 is deferred): a player
 * carries one weapon per slot. The number keys select them (DefaultInput.ini: PrimaryWeapon, SecondaryWeapon,
 * Grenade).
 */
UENUM()
enum class EShooterWeaponSlot : uint8
{
	/** Rifles and sniper rifles (CS slot 1). */
	Primary,
	/** Pistols (CS slot 2). */
	Secondary,
	/** Grenades (CS slot 4). */
	Grenade,
};

/** What a weapon is doing (UE ShooterGame: EWeaponState). */
UENUM()
enum class EShooterWeaponState : uint8
{
	Idle,
	Firing,
	Reloading,
	Equipping,
};

/** Where a shot struck a character (CS: the hit groups; Leon's pawn is a capsule, so only the head is told apart). */
UENUM()
enum class EShooterHitGroup : uint8
{
	Body,
	/** The top of the capsule (AShooterCharacter::HeadshotHeight). */
	Head,
};

/**
 * A team's tag: the PlayerStartTag of its starts and the tag of its buy zone in de_leon (plan decision D15: the map
 * carries the game's meaning as tags). NAME_None for None.
 */
[[nodiscard]] SHOOTERGAME_API FName GetShooterTeamTag(EShooterTeam Team);

/** The team a name stands for ("CT" or "T", any case), None otherwise. */
[[nodiscard]] SHOOTERGAME_API EShooterTeam ParseShooterTeam(const FString& Text);

/** The team's display name ("CT", "T", "None"). */
[[nodiscard]] SHOOTERGAME_API const TCHAR* GetShooterTeamName(EShooterTeam Team);
