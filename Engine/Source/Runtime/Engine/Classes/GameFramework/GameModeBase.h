#pragma once

#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Info.h"
#include "Templates/SubclassOf.h"
#include "GameModeBase.generated.h"

class ACharacter;
class AController;
class AHUD;
class APawn;
class APlayerController;
class APlayerState;
class UGameEngine;
class UPlayer;

/**
 * Level gameplay rules (UE: AGameModeBase), an AInfo the world spawns (UWorld::SetGameMode, with plan decision D18's
 * precedence) and keeps in AuthorityGameMode. Games subclass this; the engine never includes game headers. The class
 * members choose what the mode spawns, as in UE.
 *
 * The UE flow, driven by UEngine::LoadMap:
 * - PreInitializeComponents spawns the game state (GameStateClass); InitGame gets the map name and the URL options.
 * - Each local player logs in: Login spawns its PlayerControllerClass controller (InitNewPlayer picks its start spot:
 *   FindPlayerStart with the URL portal as the PlayerStartTag), PostLogin registers its player state and calls
 *   HandleStartingNewPlayer, which restarts it: RestartPlayer spawns DefaultPawnClass at the start (only its yaw),
 *   possesses it and turns the control rotation to the start's rotation.
 * - UWorld::BeginPlay calls StartPlay (the game state begins play).
 *
 * Until the second stage of P13 (input by config) the default game mode still has Leon's engine hooks: OnEnter once the
 * map plays, Tick(Engine, DeltaTime) every frame (which ticks the world) and OnExit before shutdown.
 */
UCLASS()
class ENGINE_API AGameModeBase : public AInfo
{
	GENERATED_BODY()

public:
	AGameModeBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnEnter(UGameEngine& Engine, const FString& LevelPath);
	virtual void OnExit(UGameEngine& Engine);
	virtual void Tick(UGameEngine& Engine, float DeltaTime);

	/** The game state class (UE: GameStateClass). */
	UPROPERTY()
	TSubclassOf<AGameStateBase> GameStateClass;

	/** The class of the players' controllers (UE: PlayerControllerClass). */
	UPROPERTY()
	TSubclassOf<APlayerController> PlayerControllerClass;

	/** The class of the players' states (UE: PlayerStateClass). */
	UPROPERTY()
	TSubclassOf<APlayerState> PlayerStateClass;

	/** The pawn a player starts with (UE: DefaultPawnClass). */
	UPROPERTY()
	TSubclassOf<APawn> DefaultPawnClass;

	/** The HUD class (UE: HUDClass; the engine keeps one HUD until the player controller owns it). */
	UPROPERTY()
	TSubclassOf<AHUD> HUDClass;

	/** The name a player without one gets, followed by its player id (UE: DefaultPlayerName). */
	UPROPERTY()
	FString DefaultPlayerName;

	/** The URL options the map was opened with (UE: OptionsString). */
	UPROPERTY(Transient)
	FString OptionsString;

	/** Spawns the game state before the components initialize (UE). */
	void PreInitializeComponents() override;

	/** The map name and the URL options, before any player logs in (UE: InitGame). */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage);

	/** The world begins play (UE: StartPlay, from UWorld::BeginPlay): the game state begins play. */
	virtual void StartPlay();

	/** Unreal InitGameState — configure the GameState once spawned. */
	virtual void InitGameState()
	{
	}

	/** Unreal StartMatch — authority begins gameplay; notifies GameState. */
	virtual void StartMatch()
	{
		GetGameState().HandleMatchHasStarted();
	}
	/** Unreal EndMatch. */
	virtual void EndMatch()
	{
		GetGameState().HandleMatchHasEnded();
	}

	/**
	 * Accepts a player (UE: Login): spawns its controller (SpawnPlayerController) and sets it up for the game
	 * (InitNewPlayer). Null with ErrorMessage when the login is refused.
	 */
	virtual APlayerController* Login(
		UPlayer* NewPlayer, const FString& Portal, const FString& Options, FString& ErrorMessage);

	/**
	 * A player finished logging in (UE: PostLogin): its player state joins the game state's PlayerArray, then
	 * HandleStartingNewPlayer.
	 */
	virtual void PostLogin(APlayerController* NewPlayer);
	/** A player leaves: its player state leaves the PlayerArray (UE: Logout). */
	virtual void Logout(AController* Exiting);

	/** A new player may start (UE: HandleStartingNewPlayer): RestartPlayer unless it cannot restart. */
	virtual void HandleStartingNewPlayer(APlayerController* NewPlayer);

	/** Whether a player may restart now (UE: PlayerCanRestart). */
	[[nodiscard]] virtual bool PlayerCanRestart(APlayerController* Player);

	/** Spawns (or keeps) the player's pawn at a start and possesses it (UE: RestartPlayer). */
	virtual void RestartPlayer(AController* NewPlayer);

	/** RestartPlayer at a given start (UE: RestartPlayerAtPlayerStart). */
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot);

	/** RestartPlayer at a transform (UE: RestartPlayerAtTransform). */
	virtual void RestartPlayerAtTransform(AController* NewPlayer, const FTransform& SpawnTransform);

	/**
	 * Where a player starts (UE: FindPlayerStart): the APlayerStart whose PlayerStartTag is IncomingName, else the
	 * player's start spot at the start of play, else ChoosePlayerStart, else the world settings.
	 */
	[[nodiscard]] virtual AActor* FindPlayerStart(AController* Player, const FString& IncomingName = FString());

	/**
	 * Picks a start (UE: ChoosePlayerStart): the first Play From Here start (APlayerStartPIE), else the first player
	 * start in spawn order (UE picks a random unoccupied one; Leon is deterministic and tests no overlap).
	 */
	[[nodiscard]] virtual AActor* ChoosePlayerStart(AController* Player);

	/** The player's start spot is used at the start of play (UE: ShouldSpawnAtStartSpot). */
	[[nodiscard]] virtual bool ShouldSpawnAtStartSpot(AController* Player);

	/** The pawn class of a controller (UE: GetDefaultPawnClassForController): DefaultPawnClass. */
	[[nodiscard]] virtual UClass* GetDefaultPawnClassForController(AController* InController);

	/** Spawns the default pawn at the start's location, turned to its yaw (UE: SpawnDefaultPawnFor). */
	virtual APawn* SpawnDefaultPawnFor(AController* NewPlayer, AActor* StartSpot);

	/** Spawns the default pawn at a transform (UE: SpawnDefaultPawnAtTransform). */
	virtual APawn* SpawnDefaultPawnAtTransform(AController* NewPlayer, const FTransform& SpawnTransform);

	/** Unreal GetNumPlayers (from GameState::PlayerArray). */
	[[nodiscard]] int GetNumPlayers() const
	{
		return GetGameState().GetNumPlayers();
	}

	/** Recreate World FPhysScene backend; the registered components' bodies come back (UWorld::SetPhysicsBackend). */
	void SetPhysicsBackend(EPhysicsBackend Backend)
	{
		GetWorld()->SetPhysicsBackend(Backend);
	}

	/**
	 * The game state. A reference (UE's GameState member is a pointer): the mode spawns it before anything can ask.
	 */
	[[nodiscard]] AGameStateBase& GetGameState()
	{
		return *GameState;
	}
	[[nodiscard]] const AGameStateBase& GetGameState() const
	{
		return *GameState;
	}

	/** Unreal GetGameState<T>(). */
	template <typename T>
	[[nodiscard]] T* GetGameState() const
	{
		static_assert(TIsDerivedFrom<T, AGameStateBase>::Value, "T must derive from GameState");
		return Cast<T>(GameState);
	}

	/** Replaces the GameState with a new T (e.g. a game-specific subclass): the old one is destroyed. */
	template <typename T>
	T* SetGameState()
	{
		static_assert(TIsDerivedFrom<T, AGameStateBase>::Value, "T must derive from GameState");
		GameStateClass = T::StaticClass();
		SpawnGameState();
		return Cast<T>(GameState);
	}

	/**
	 * Ticks the game state (Leon: AInfo actors do not tick in the world, and the game state keeps the match clock).
	 */
	void Tick(float DeltaSeconds) override;

	/** Min APlayerStart Z, or 0 if none. */
	[[nodiscard]] static float EstimateFloorZ(const ULevel& Level);
	/** Soft XY walk clamp from the scale of the static collision primitives (cm, clamped 2000–12000). */
	[[nodiscard]] static float EstimateWalkBounds(const ULevel& Level);

protected:
	/** Spawns the player's controller (UE: SpawnPlayerController): PlayerControllerClass, transient. */
	virtual APlayerController* SpawnPlayerController(const FString& Options);

	/**
	 * Sets up a logged-in controller (UE: InitNewPlayer): its start spot (UpdatePlayerStartSpot with the portal) and
	 * its name (`?Name=`, else DefaultPlayerName and the player id). Returns an error message, empty on success.
	 */
	virtual FString InitNewPlayer(
		APlayerController* NewPlayerController, const FString& Options, const FString& Portal = FString());

	/** Finds the start spot and places the controller there (UE: UpdatePlayerStartSpot). */
	virtual bool UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage);

	/** Possesses the pawn and turns the control rotation to the start's (UE: FinishRestartPlayer). */
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation);

	/** The restart failed (UE: FailedToRestartPlayer). */
	virtual void FailedToRestartPlayer(AController* NewPlayer);

	/** Shared set-up of a joining player (UE: GenericPlayerInitialization). */
	virtual void GenericPlayerInitialization(AController* C);

	// Flow: Match enter — bodies + nav bake
	void PrepareMatchWorld(float& OutFloorZ, float& OutWalkBounds, EPhysicsBackend Backend = EPhysicsBackend::Jolt);
	void RebuildNavigation(float FloorZ, float WalkBounds);
	void SnapCharacterToFloor(ACharacter& Character, FVector& InOutFeet, float FloorZ) const;

	/** Spawns a GameStateClass game state, destroying the current one, then InitGameState (UE). */
	void SpawnGameState();

	/** The game state (UE: GameState). */
	UPROPERTY(Transient)
	AGameStateBase* GameState = nullptr;
};
