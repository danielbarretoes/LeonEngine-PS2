#pragma once

#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "LegacyCoordinateConversion.h"

#include <string_view>

class ACharacter;
class UGameEngine;
class APlayerController;

/**
 * Level gameplay rules (Unreal-style AGameModeBase / AGameMode).
 * Games subclass this; the engine never includes game headers.
 */
class ENGINE_API AGameModeBase
{
public:
	virtual ~AGameModeBase() = default;

	AGameModeBase(const AGameModeBase&) = delete;
	AGameModeBase& operator=(const AGameModeBase&) = delete;
	AGameModeBase(AGameModeBase&&) = delete;
	AGameModeBase& operator=(AGameModeBase&&) = delete;

	virtual void OnEnter(UGameEngine& Engine, const FString& LevelPath) = 0;
	virtual void OnExit(UGameEngine& Engine) = 0;
	virtual void Tick(UGameEngine& Engine, float DeltaTime) = 0;

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

	[[nodiscard]] UWorld& GetWorld()
	{
		return World;
	}
	[[nodiscard]] const UWorld& GetWorld() const
	{
		return World;
	}

	/** Recreate World FPhysScene backend (clears bodies). Prefer before RegisterBodiesFromLevel. */
	void SetPhysicsBackend(EPhysicsBackend Backend)
	{
		World.SetPhysicsBackend(Backend);
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
	AGameModeBase()
		: GameState(MakeUnique<AGameStateBase>())
	{
	}

	/** Framework helper: Level collision meshes → World FPhysScene (not game rules). */
	void RegisterBodiesFromLevel(const ULevel& Level)
	{
		GetWorld().RegisterBodiesFromLevel(Level);
	}

	/** Unreal FindPlayerStart — resolve spawn transform (slot picks among starts); the yaw is the start's UE yaw. */
	[[nodiscard]] bool FindPlayerStart(
		const ULevel& Level, FVector& OutLocation, float& OutYawDegrees, int Slot = 0) const
	{
		const auto& Starts = Level.GetPlayerStarts();
		if (Starts.Num() == 0)
		{
			OutLocation = {0.0f, 0.0f, 0.0f};
			OutYawDegrees = 0.0f;
			return false;
		}
		const int Index = FMath::Clamp(Slot, 0, static_cast<int>(Starts.Num()) - 1);
		const FPlayerStart& Start = Starts[Index];
		OutLocation = Start.Transform.GetLocation();
		OutYawDegrees = Start.Transform.Rotator().Yaw;
		return true;
	}

	// Flow: Match enter — bodies + nav bake
	void PrepareMatchWorld(
		UGameEngine& Engine, float& OutFloorZ, float& OutWalkBounds, EPhysicsBackend Backend = EPhysicsBackend::Jolt);
	void RebuildNavigation(UGameEngine& Engine, float FloorZ, float WalkBounds);
	void SnapCharacterToFloor(ACharacter& Character, FVector& InOutFeet, float FloorZ) const;

private:
	UWorld World;
	TUniquePtr<AGameStateBase> GameState;
};
