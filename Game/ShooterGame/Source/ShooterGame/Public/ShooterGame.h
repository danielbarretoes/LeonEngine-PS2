#pragma once

#include "CoreMinimal.h"

/** ShooterGame's log (UE ShooterGame: LogShooter). */
SHOOTERGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogShooter, Log, All);

/**
 * The channel the weapons trace on (UE ShooterGame: COLLISION_WEAPON): the project's Weapon channel
 * (DefaultEngine.ini's ECC_GameTraceChannel1), blocked by every body, the characters' capsules included.
 */
#define COLLISION_WEAPON ECC_GameTraceChannel1
