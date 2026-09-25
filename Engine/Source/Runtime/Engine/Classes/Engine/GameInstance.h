#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "Misc/Exec.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "GameInstance.generated.h"

class AGameModeBase;
class APlayerController;
class UEngine;
class UGameInstance;
class ULocalPlayer;
class UPackage;
class UWorld;

/**
 * The world a game instance plays in, with what the engine knows about it (UE: FWorldContext). UE keeps the contexts
 * in UEngine::WorldList and each game instance points at its own; Leon keeps P12's ownership (the game instance owns
 * its context) and UEngine::GetWorldContexts collects them.
 */
USTRUCT()
struct ENGINE_API FWorldContext
{
	GENERATED_BODY()

	/** What the context's world is for (UE: WorldType). */
	EWorldType::Type WorldType = EWorldType::None;

	/** The game instance this context belongs to (UE: OwningGameInstance). */
	UPROPERTY(Transient)
	UGameInstance* OwningGameInstance = nullptr;

	/** A name that identifies the context (UE: ContextHandle). */
	UPROPERTY(Transient)
	FName ContextHandle;

	/** The URL of the map loaded last (UE: LastURL). */
	UPROPERTY(Transient)
	FURL LastURL;

	/** A travel UEngine::TickWorldTravel browses next frame (UE: TravelURL / TravelType; SetClientTravel). */
	UPROPERTY(Transient)
	FString TravelURL;

	uint8 TravelType = 0;

	/** The world (UE: World()). */
	[[nodiscard]] UWorld* World() const
	{
		return ThisCurrentWorld;
	}

	/** Replaces the world (UE: SetCurrentWorld); the world's OwningGameInstance follows. */
	void SetCurrentWorld(UWorld* World);

private:
	/** UE: ThisCurrentWorld. */
	UPROPERTY(Transient)
	UWorld* ThisCurrentWorld = nullptr;
};

/**
 * The game session (UE: UGameInstance), created by the engine (GameInstanceClass of UGameMapsSettings) inside it and
 * kept across map changes. It owns its world context (Leon, P12) and the local players.
 *
 * - InitializeStandalone creates the context with a placeholder world; StartGameInstance opens the first map: the
 *   first command-line token (a map name or path), `-map=` (Leon's alias, kept for the scripts), else
 *   UGameMapsSettings::GetGameDefaultMap. A map that cannot be opened is an error and asks the engine to exit.
 * - CreateGameModeForURL picks the game mode class with plan decision D18's precedence and spawns it.
 */
UCLASS(Transient)
class ENGINE_API UGameInstance
	: public UObject
	, public FExec
{
	GENERATED_BODY()

public:
	/** Called once the engine is initialized (UE). Overrides call Super. */
	virtual void Init();
	/** Called when the engine shuts down, before its world is destroyed (UE). Overrides call Super. */
	virtual void Shutdown();

	/**
	 * Creates the world context and a placeholder game world in a transient package (UE: InitializeStandalone);
	 * UEngine::LoadMap replaces the world with a map.
	 */
	void InitializeStandalone(FName InPackageName = NAME_None, UPackage* InWorldPackage = nullptr);

	/** Opens the first map (UE: StartGameInstance; see the class comment). */
	virtual void StartGameInstance();

	/**
	 * Spawns the game mode of InURL's world (UE: CreateGameModeForURL), with plan decision D18's precedence:
	 * `?game=<Class or alias>` in the URL, then the world settings' DefaultGameMode, then the map name's
	 * GameModeMapPrefixes, then UGameMapsSettings's GlobalDefaultGameMode, then AGameModeBase. A `?game=` class that
	 * cannot be loaded is a warning and the next step decides.
	 */
	virtual AGameModeBase* CreateGameModeForURL(FURL InURL, UWorld* InWorld);

	/** The class CreateGameModeForURL settled on, which a game instance may replace (UE: OverrideGameModeClass). */
	virtual TSubclassOf<AGameModeBase> OverrideGameModeClass(TSubclassOf<AGameModeBase> GameModeClass,
		const FString& MapName, const FString& Options, const FString& Portal) const;

	/** Called by UEngine::LoadMap once a map plays (UE: LoadComplete); counts it (NotifyLevelOpened). */
	virtual void LoadComplete(const float LoadTime, const FString& MapName);

	/**
	 * Ends play in the context's world, destroys it and clears the context (Leon: UE's engine does this on exit and on
	 * LoadMap). The caller collects garbage afterwards.
	 */
	void DestroyWorldContextWorld();

	/** The first local player (UE: CreateInitialPlayer): CreateLocalPlayer(0) without a controller. */
	ULocalPlayer* CreateInitialPlayer(FString& OutError);

	/**
	 * A new local player of the engine's LocalPlayerClass (UE: CreateLocalPlayer); with bSpawnPlayerController and a
	 * world it logs in at once.
	 */
	ULocalPlayer* CreateLocalPlayer(int32 ControllerId, FString& OutError, bool bSpawnPlayerController);

	/** Adds a local player and tells it its viewport client (UE: AddLocalPlayer); returns its index. */
	virtual int32 AddLocalPlayer(ULocalPlayer* NewPlayer, int32 ControllerId);

	/** Removes a local player; its controller is destroyed (UE: RemoveLocalPlayer). */
	virtual bool RemoveLocalPlayer(ULocalPlayer* ExistingPlayer);

	[[nodiscard]] const TArray<ULocalPlayer*>& GetLocalPlayers() const
	{
		return LocalPlayers;
	}
	[[nodiscard]] int32 GetNumLocalPlayers() const
	{
		return LocalPlayers.Num();
	}
	/** UE: GetFirstGamePlayer. */
	[[nodiscard]] ULocalPlayer* GetFirstGamePlayer() const;
	/** The first local player's controller in World (the context's world when null) (UE). */
	[[nodiscard]] APlayerController* GetFirstLocalPlayerController(const UWorld* World = nullptr) const;

	/** The engine that owns this game instance (its outer), or null (UE: GetEngine). */
	[[nodiscard]] UEngine* GetEngine() const;

	[[nodiscard]] FWorldContext* GetWorldContext()
	{
		return &WorldContext;
	}
	[[nodiscard]] const FWorldContext* GetWorldContext() const
	{
		return &WorldContext;
	}
	/** The context's world (UE: GetWorld). */
	[[nodiscard]] UWorld* GetWorld() const
	{
		return WorldContext.World();
	}

	/** No console commands of its own (UE: UGameInstance::Exec); its Exec UFUNCTIONs answer ProcessConsoleExec. */
	bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/** Called when a map starts playing (Leon; LoadComplete calls it). */
	virtual void NotifyLevelOpened()
	{
		++LevelsOpened;
	}

	[[nodiscard]] int32 GetLevelsOpened() const
	{
		return LevelsOpened;
	}

private:
	/** The world context (UE: WorldContext, a pointer into GEngine->WorldList there). */
	UPROPERTY(Transient)
	FWorldContext WorldContext;

	/** The local players (UE: LocalPlayers). */
	UPROPERTY(Transient)
	TArray<ULocalPlayer*> LocalPlayers;

	int32 LevelsOpened = 0;
};
