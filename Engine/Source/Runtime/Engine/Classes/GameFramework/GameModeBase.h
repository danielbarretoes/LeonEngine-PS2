#pragma once

#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Info.h"
#include "GameModeBase.generated.h"

class ACharacter;
class UGameEngine;
class APlayerController;

/**
 * Level gameplay rules (UE: AGameModeBase), an AInfo the world spawns (UWorld::SetGameMode) and keeps in
 * AuthorityGameMode. Games subclass this; the engine never includes game headers.
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

	/** The actor tick (the world ticks the game mode like any actor); the engine hook below is another overload. */
	using Super::Tick;

	virtual void OnEnter(UGameEngine& Engine, const FString& LevelPath);
	virtual void OnExit(UGameEngine& Engine);
	virtual void Tick(UGameEngine& Engine, float DeltaTime);

	/** Unreal InitGameState — configure / replace GameState after construction. */
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
	/** Unreal RestartPlayer — spawn/possess pawn at a FPlayerStart (games override). */
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
	[[nodiscard]] T* GetGameState()
	{
		static_assert(TIsDerivedFrom<T, AGameStateBase>::Value, "T must derive from GameState");
		return dynamic_cast<T*>(GameState.Get());
	}
	template <typename T>
	[[nodiscard]] const T* GetGameState() const
	{
		static_assert(TIsDerivedFrom<T, AGameStateBase>::Value, "T must derive from GameState");
		return dynamic_cast<const T*>(GameState.Get());
	}

	/** Replace the GameState instance (e.g. game-specific subclass). Calls InitGameState. */
	template <typename T, typename... ArgsType>
	T* SetGameState(ArgsType&&... Args)
	{
		static_assert(TIsDerivedFrom<T, AGameStateBase>::Value, "T must derive from GameState");
		auto Owned = MakeUnique<T>(Forward<ArgsType>(Args)...);
		T* Raw = Owned.Get();
		GameState = MoveTemp(Owned);
		InitGameState();
		return Raw;
	}

	/** Min FPlayerStart Z, or 0 if none. */
	[[nodiscard]] static float EstimateFloorZ(const ULevel& Level);
	/** Soft XY walk clamp from static mesh extents (cm, clamped 2000–12000). */
	[[nodiscard]] static float EstimateWalkBounds(const ULevel& Level);

protected:
	/** Framework helper: Level collision meshes → World FPhysScene (not game rules). */
	void RegisterBodiesFromLevel(const ULevel& Level)
	{
		GetWorld()->RegisterBodiesFromLevel(Level);
	}

	/**
	 * Unreal FindPlayerStart — resolve the spawn transform (slot picks among starts). Like UE's spawn at a start, only
	 * the start's yaw is kept.
	 */
	[[nodiscard]] bool FindPlayerStart(
		const ULevel& Level, FVector& OutLocation, FRotator& OutRotation, int Slot = 0) const
	{
		const auto& Starts = Level.GetPlayerStarts();
		if (Starts.Num() == 0)
		{
			OutLocation = {0.0f, 0.0f, 0.0f};
			OutRotation = FRotator::ZeroRotator;
			return false;
		}
		const int Index = FMath::Clamp(Slot, 0, static_cast<int>(Starts.Num()) - 1);
		const FPlayerStart& Start = Starts[Index];
		OutLocation = Start.Transform.GetLocation();
		OutRotation = FRotator(0.0f, Start.Transform.Rotator().Yaw, 0.0f);
		return true;
	}

	// Flow: Match enter — bodies + nav bake
	void PrepareMatchWorld(
		UGameEngine& Engine, float& OutFloorZ, float& OutWalkBounds, EPhysicsBackend Backend = EPhysicsBackend::Jolt);
	void RebuildNavigation(UGameEngine& Engine, float FloorZ, float WalkBounds);
	void SnapCharacterToFloor(ACharacter& Character, FVector& InOutFeet, float FloorZ) const;

private:
	TUniquePtr<AGameStateBase> GameState;
};
