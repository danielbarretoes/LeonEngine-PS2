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
 * The phase of a round (Counter-Strike's round flow; AShooterGameMode drives it, AShooterGameState holds it). Warmup
 * lasts until both teams have a player; each round is Freeze (nobody moves, buying), Live (the round's time), then
 * RoundEnd (the result shows) before the next Freeze; MatchEnd after the last round.
 */
UENUM()
enum class EShooterRoundState : uint8
{
	Warmup,
	Freeze,
	Live,
	RoundEnd,
	MatchEnd,
};

/** Where the bomb is (AShooterBomb). */
UENUM()
enum class EShooterBombState : uint8
{
	/** Not in play (no round, or no terrorist to carry it). */
	None,
	/** A terrorist carries it. */
	Carried,
	/** On the floor, for a terrorist to pick up. */
	Dropped,
	/** Planted in a bomb site, ticking. */
	Planted,
	Defused,
	Exploded,
};

/** Why a round ended (Counter-Strike's round end messages). */
UENUM()
enum class EShooterRoundEndReason : uint8
{
	None,
	/** "Target Successfully Bombed!": the planted bomb exploded (T). */
	TargetBombed,
	/** "The bomb has been defused!" (CT). */
	BombDefused,
	/** "Terrorists Win!": every CT is dead (T). */
	CTsEliminated,
	/** "Counter-Terrorists Win!": every T is dead with no bomb planted (CT). */
	TerroristsEliminated,
	/** "Target has been saved!": the time ran out with no bomb planted (CT). */
	TargetSaved,
	/** "Round Draw!": both teams died at once (nobody scores). */
	Draw,
};

/** The other team (CT for T, T for CT, None for None). */
[[nodiscard]] SHOOTERGAME_API EShooterTeam GetOpposingTeam(EShooterTeam Team);

/** Counter-Strike's message for a round's end ("Target Successfully Bombed!", ...). */
[[nodiscard]] SHOOTERGAME_API const TCHAR* GetRoundEndMessage(EShooterRoundEndReason Reason);

/** The team a round end reason gives the round to (None for a draw). */
[[nodiscard]] SHOOTERGAME_API EShooterTeam GetRoundEndWinner(EShooterRoundEndReason Reason);

/**
 * A team's tag: the PlayerStartTag of its starts and the tag of its buy zone in de_leon (plan decision D15: the map
 * carries the game's meaning as tags). NAME_None for None.
 */
[[nodiscard]] SHOOTERGAME_API FName GetShooterTeamTag(EShooterTeam Team);

/** The team a name stands for ("CT" or "T", any case), None otherwise. */
[[nodiscard]] SHOOTERGAME_API EShooterTeam ParseShooterTeam(const FString& Text);

/** The team's display name ("CT", "T", "None"). */
[[nodiscard]] SHOOTERGAME_API const TCHAR* GetShooterTeamName(EShooterTeam Team);
