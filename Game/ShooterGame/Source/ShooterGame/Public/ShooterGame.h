#pragma once

#include "CoreMinimal.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

/** ShooterGame's log (UE ShooterGame: LogShooter). */
SHOOTERGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogShooter, Log, All);

/**
 * The channel the weapons trace on (UE ShooterGame: COLLISION_WEAPON): the project's Weapon channel
 * (DefaultEngine.ini's ECC_GameTraceChannel1), blocked by every body, the characters' capsules included.
 */
#define COLLISION_WEAPON ECC_GameTraceChannel1

/**
 * The surfaces of the physical materials (UE ShooterGame: SHOOTER_SURFACE_*), named in DefaultEngine.ini's
 * `[/Script/Engine.PhysicsSettings]`: Counter-Strike's texture types (CHAR_TEX_*) the game needs. The penetration of a
 * bullet, the sound of a step and of an impact, and the impact's mark follow them.
 */
#define SHOOTER_SURFACE_Default SurfaceType_Default
#define SHOOTER_SURFACE_Concrete SurfaceType1
#define SHOOTER_SURFACE_Dirt SurfaceType2
#define SHOOTER_SURFACE_Metal SurfaceType3
#define SHOOTER_SURFACE_Wood SurfaceType4
#define SHOOTER_SURFACE_Tile SurfaceType5
#define SHOOTER_SURFACE_Glass SurfaceType6
#define SHOOTER_SURFACE_Computer SurfaceType7
#define SHOOTER_SURFACE_Flesh SurfaceType8
