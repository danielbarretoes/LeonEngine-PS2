#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Templates/SubclassOf.h"
#include "WorldSettings.generated.h"

class AGameModeBase;

/**
 * Per-level settings (UE: AWorldSettings), an AInfo in the level that ULevel::GetWorldSettings returns. The `.llev`
 * reader spawns one first. Its legacy fields with no UE home (the level name, the game mode string, the ignored
 * environment map and the camera framing) live in the ULegacyLevelDataComponent the reader gives it.
 */
UCLASS(NotPlaceable)
class ENGINE_API AWorldSettings : public AInfo
{
	GENERATED_BODY()

public:
	AWorldSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * The game mode this level asks for (UE: DefaultGameMode), after `?game=` and before the project's
	 * GlobalDefaultGameMode (plan decision D18). The `.llev` game mode is a free string, so levels read from it leave
	 * this empty.
	 */
	UPROPERTY()
	TSubclassOf<AGameModeBase> DefaultGameMode;

	/** Height below which actors are killed, cm (UE: KillZ; the default is UE's, -HALF_WORLD_MAX1). */
	UPROPERTY()
	float KillZ = -1048575.0f;
};
