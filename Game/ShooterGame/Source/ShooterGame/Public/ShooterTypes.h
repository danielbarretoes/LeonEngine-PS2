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
 * A team's tag: the PlayerStartTag of its starts and the tag of its buy zone in de_leon (plan decision D15: the map
 * carries the game's meaning as tags). NAME_None for None.
 */
[[nodiscard]] SHOOTERGAME_API FName GetShooterTeamTag(EShooterTeam Team);

/** The team a name stands for ("CT" or "T", any case), None otherwise. */
[[nodiscard]] SHOOTERGAME_API EShooterTeam ParseShooterTeam(const FString& Text);

/** The team's display name ("CT", "T", "None"). */
[[nodiscard]] SHOOTERGAME_API const TCHAR* GetShooterTeamName(EShooterTeam Team);
