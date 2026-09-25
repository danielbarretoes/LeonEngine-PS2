#pragma once

#include "CoreMinimal.h"

/** General engine messages (UE: LogEngine). */
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogEngine, Log, All);
/** Level loading and level data (UE: LogLevel). */
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogLevel, Log, All);
/** Navigation and pathfinding (UE: LogPath). */
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogPath, Log, All);
/** Collision and physics scene (UE: LogPhysics). */
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogPhysics, Log, All);
/** Actor spawning (UE: LogSpawn). */
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogSpawn, Log, All);
/** World creation and teardown (UE: LogWorld). */
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogWorld, Log, All);
/** Map loading (UE: LogLoad). */
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogLoad, Log, All);
/** Game mode flow: login, restart, player starts (UE: LogGameMode). */
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogGameMode, Log, All);
