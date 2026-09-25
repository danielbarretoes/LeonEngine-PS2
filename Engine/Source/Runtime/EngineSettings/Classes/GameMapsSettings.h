#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "GameMapsSettings.generated.h"

/** A short name for a game mode class (UE: FGameModeName): `?game=<Name>` and the map prefixes use it. */
USTRUCT()
struct ENGINESETTINGS_API FGameModeName
{
	GENERATED_BODY()

	/** The alias or the map prefix (compared without case). */
	UPROPERTY()
	FString Name;

	/** The game mode class it stands for. */
	UPROPERTY()
	FSoftClassPath GameMode;
};

/**
 * The maps and game modes a game starts with (UE: UGameMapsSettings), read from
 * [/Script/EngineSettings.GameMapsSettings] of the Engine config: BaseEngine.ini, then the project's DefaultEngine.ini.
 *
 * - GameDefaultMap: the map LeonGame opens when the command line names none (UGameInstance::StartGameInstance).
 * - GlobalDefaultGameMode: the last step of the game mode precedence (plan decision D18): `?game=` in the URL, then the
 *   level's AWorldSettings::DefaultGameMode, then this (UGameInstance::CreateGameModeForURL).
 * - GameInstanceClass: the class of the engine's game instance.
 *
 * Until the `.lmap` packages (P15) a map is a long package name whose `.llev` file sits under a mount point
 * (`/Engine/LevelTemplates/Starter` is Engine/Content/LevelTemplates/Starter.llev), or a `.llev` path.
 */
UCLASS(Config = Engine, DefaultConfig)
class ENGINESETTINGS_API UGameMapsSettings : public UObject
{
	GENERATED_BODY()

public:
	UGameMapsSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The map a game starts with (UE: GetGameDefaultMap; Leon has no dedicated server, so always GameDefaultMap). */
	static FString GetGameDefaultMap();

	/** The global default game mode class path (UE: GetGlobalDefaultGameMode). */
	static FString GetGlobalDefaultGameMode();

	/** The class path an alias of GameModeClassAliases stands for, or GameModeName itself (UE: GetGameModeForName). */
	static FString GetGameModeForName(const FString& GameModeName);

	/**
	 * The game mode of the first GameModeMapPrefixes entry the map's name starts with, or an empty string (UE:
	 * GetGameModeForMapName).
	 */
	static FString GetGameModeForMapName(const FString& MapName);

	/** Changes the default map of the class defaults (UE: SetGameDefaultMap; not saved). */
	static void SetGameDefaultMap(const FString& NewMap);

	/** Changes the global default game mode of the class defaults (UE: SetGlobalDefaultGameMode; not saved). */
	static void SetGlobalDefaultGameMode(const FString& NewGameMode);

	/** Options appended to the default map's URL (UE: LocalMapOptions, e.g. "?listen"). */
	UPROPERTY(Config)
	FString LocalMapOptions;

	/** The game instance class the engine creates (UE: GameInstanceClass). */
	UPROPERTY(Config)
	FSoftClassPath GameInstanceClass;

	/** Map prefixes that pick a game mode when neither the URL nor the level names one (UE: GameModeMapPrefixes). */
	UPROPERTY(Config)
	TArray<FGameModeName> GameModeMapPrefixes;

	/** Short names for `?game=` (UE: GameModeClassAliases). */
	UPROPERTY(Config)
	TArray<FGameModeName> GameModeClassAliases;

private:
	/** The map a game opens (UE: GameDefaultMap). */
	UPROPERTY(Config)
	FSoftObjectPath GameDefaultMap;

	/** The map a dedicated server opens (UE: ServerDefaultMap; Leon has no dedicated server yet). */
	UPROPERTY(Config)
	FSoftObjectPath ServerDefaultMap;

	/** The game mode used when the URL and the level name none (UE: GlobalDefaultGameMode). */
	UPROPERTY(Config)
	FSoftClassPath GlobalDefaultGameMode;
};
