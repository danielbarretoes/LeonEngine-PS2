#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Templates/SubclassOf.h"
#include "WorldSettings.generated.h"

class AGameModeBase;

/**
 * How LeonEd bakes a map's static lighting (UE: FLightmassWorldInfoSettings, the world settings' Lightmass settings;
 * Docs/PLANS/ps2-shipping.md N22). The environment is a uniform sky around the map: each vertex receives
 * EnvironmentColor x EnvironmentIntensity times the share of its hemisphere (cosine-weighted) that sees no geometry
 * within MaxOcclusionDistance, its ambient occlusion. Objects lit per frame (Movable) take the same environment,
 * without occlusion, as their ambient light.
 */
USTRUCT()
struct ENGINE_API FLightmassWorldInfoSettings
{
	GENERATED_BODY()

	/** The sky's colour (UE: EnvironmentColor; a light blue here, UE's is black). */
	UPROPERTY()
	FLinearColor EnvironmentColor = FLinearColor(0.78f, 0.85f, 1.0f, 1.0f);

	/** Scales EnvironmentColor (UE: EnvironmentIntensity). */
	UPROPERTY()
	float EnvironmentIntensity = 0.35f;

	/** Occludes the sky by the map's geometry (UE: bUseAmbientOcclusion). */
	UPROPERTY()
	bool bUseAmbientOcclusion = true;

	/** Geometry further than this does not occlude, cm (UE: MaxOcclusionDistance). */
	UPROPERTY()
	float MaxOcclusionDistance = 300.0f;

	/** The rays a vertex casts over its hemisphere for the occlusion (Leon; fixed directions, the same every bake). */
	UPROPERTY()
	int32 NumOcclusionRays = 64;

	/** The ambient light of an unoccluded point: EnvironmentColor x EnvironmentIntensity (RGB). */
	[[nodiscard]] FVector GetEnvironmentLight() const
	{
		return FVector(EnvironmentColor.R, EnvironmentColor.G, EnvironmentColor.B) * EnvironmentIntensity;
	}
};

/**
 * A map's distance fog (Leon, Docs/PLANS/ps2-shipping.md N15; UE: AExponentialHeightFog, whose StartDistance and
 * FogInscatteringColor it keeps): the GS's fog (FOGCOL, a coefficient per vertex), linear in the view's depth from
 * StartDistance, where nothing is fogged, to EndDistance, where everything is the fog's colour. The world pass's
 * meshes, impact marks, effect sprites and blob shadows are fogged; the view model, the tracers and the HUD are not.
 */
USTRUCT()
struct ENGINE_API FWorldFogSettings
{
	GENERATED_BODY()

	/** Draws the fog (off: the GS's FGE stays off, as before N15). */
	UPROPERTY()
	bool bEnableFog = false;

	/** The fog's colour (UE: FogInscatteringColor; linear RGB, 1 = 255). */
	UPROPERTY()
	FLinearColor FogInscatteringColor = FLinearColor(0.45f, 0.5f, 0.55f, 1.0f);

	/** The view depth where the fog starts, cm (UE: StartDistance). */
	UPROPERTY()
	float StartDistance = 1500.0f;

	/** The view depth where everything is the fog's colour, cm (Leon; UE's fog is exponential, with FogCutoffDistance).
	 */
	UPROPERTY()
	float EndDistance = 6000.0f;
};

/**
 * Per-level settings (UE: AWorldSettings), an AInfo in the level that ULevel::GetWorldSettings returns: the first
 * actor of a map, saved with it.
 */
UCLASS(NotPlaceable)
class ENGINE_API AWorldSettings : public AInfo
{
	GENERATED_BODY()

public:
	AWorldSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * The game mode this level asks for (UE: DefaultGameMode), after `?game=` and before the project's
	 * GlobalDefaultGameMode (plan decision D18); empty leaves it to the project.
	 */
	UPROPERTY()
	TSubclassOf<AGameModeBase> DefaultGameMode;

	/** Height below which actors are killed, cm (UE: KillZ; the default is UE's, -HALF_WORLD_MAX1). */
	UPROPERTY()
	float KillZ = -1048575.0f;

	/** The world's gravity, cm/s^2; 0 takes UWorld::DefaultGravityZ (UE: GlobalGravityZ with bGlobalGravitySet). */
	UPROPERTY()
	float GlobalGravityZ = 0.0f;

	/** How the map's static lighting is baked, and the ambient light of what is lit per frame (UE: LightmassSettings).
	 */
	UPROPERTY()
	FLightmassWorldInfoSettings LightmassSettings;

	/** The map's distance fog (Leon, N15; off by default). */
	UPROPERTY()
	FWorldFogSettings FogSettings;
};
