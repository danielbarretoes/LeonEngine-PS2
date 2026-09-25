#pragma once

#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Info.h"
#include "Templates/SubclassOf.h"
#include "GameModeBase.generated.h"

class ACharacter;
class AHUD;
class APawn;
class APlayerController;
class APlayerState;
class UGameEngine;

/**
 * Level gameplay rules (UE: AGameModeBase), an AInfo the world spawns (UWorld::SetGameMode) and keeps in
 * AuthorityGameMode. Games subclass this; the engine never includes game headers. PreInitializeComponents spawns the
 * game state (GameStateClass); the class members choose what the mode spawns, as in UE.
 *
 * Until P13 (UEngine::LoadMap, input by config) FGameApplication drives it through three engine hooks: OnEnter once
 * the map is loaded, Tick(Engine, DeltaTime) every frame (which ticks the world) and OnExit before shutdown.
 */
UCLASS()
class ENGINE_API AGameModeBase : public AInfo
{
	GENERATED_BODY()

public:
	AGameModeBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** AActor::Tick (an AInfo: the world does not tick it); the engine hook below is another overload. */
	using Super::Tick;

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

	/** The HUD class (UE: HUDClass; the engine keeps one HUD until P13's viewport client). */
	UPROPERTY()
	TSubclassOf<AHUD> HUDClass;

	/** Spawns the game state before the components initialize (UE). */
	void PreInitializeComponents() override;

	/** The world begins play for this mode (UE: StartPlay, from UWorld::BeginPlay or SetGameMode). */
	virtual void StartPlay()
	{
	}

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

	/** Unreal AGameModeBase::PostLogin — registers PlayerState on GameState::PlayerArray. */
	virtual void PostLogin(APlayerController& NewPlayer);
	/** Unreal AGameModeBase::Logout — removes PlayerState from GameState::PlayerArray. */
	virtual void Logout(APlayerController& Exiting);
	/** Unreal RestartPlayer — spawn/possess pawn at an APlayerStart (games override). */
	virtual void RestartPlayer(APlayerController& /*newPlayer*/)
	{
	}
	/** Unreal HandleStartingNewPlayer — default calls RestartPlayer. */
	virtual void HandleStartingNewPlayer(APlayerController& NewPlayer)
	{
		RestartPlayer(NewPlayer);
	}

	/** Unreal GetNumPlayers (from GameState::PlayerArray). */
	[[nodiscard]] int GetNumPlayers() const
	{
		return GetGameState().GetNumPlayers();
	}

	/** Recreate World FPhysScene backend (clears bodies). Prefer before RegisterBodiesFromLevel. */
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

	/** Min APlayerStart Z, or 0 if none. */
	[[nodiscard]] static float EstimateFloorZ(const ULevel& Level);
	/** Soft XY walk clamp from the scale of the static collision primitives (cm, clamped 2000–12000). */
	[[nodiscard]] static float EstimateWalkBounds(const ULevel& Level);

protected:
	/** Framework helper: the level's collision primitives → World FPhysScene (not game rules). */
	void RegisterBodiesFromLevel(const ULevel& Level)
	{
		GetWorld()->RegisterBodiesFromLevel(Level);
	}

	/**
	 * Unreal FindPlayerStart — resolve the spawn transform from the level's APlayerStart actors (slot picks among them,
	 * in spawn order). Like UE's spawn at a start, only the start's yaw is kept.
	 */
	[[nodiscard]] bool FindPlayerStart(
		const ULevel& Level, FVector& OutLocation, FRotator& OutRotation, int Slot = 0) const;

	// Flow: Match enter — bodies + nav bake
	void PrepareMatchWorld(
		UGameEngine& Engine, float& OutFloorZ, float& OutWalkBounds, EPhysicsBackend Backend = EPhysicsBackend::Jolt);
	void RebuildNavigation(UGameEngine& Engine, float FloorZ, float WalkBounds);
	void SnapCharacterToFloor(ACharacter& Character, FVector& InOutFeet, float FloorZ) const;

	/** Spawns a GameStateClass game state, destroying the current one, then InitGameState (UE). */
	void SpawnGameState();

	/** The game state (UE: GameState). */
	UPROPERTY(Transient)
	AGameStateBase* GameState = nullptr;
};
