#pragma once

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Level/LevelCatalog.h"

#include <glm/vec3.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

class ACharacter;
class UGameEngine;
class APlayerController;

/// Pluggable level gameplay rules (Unreal-style `AGameModeBase` / `AGameMode`).
/// Packs subclass this; the engine never includes pack headers.
class ENGINE_API AGameModeBase
{
public:
	virtual ~AGameModeBase() = default;

	AGameModeBase(const AGameModeBase&) = delete;
	AGameModeBase& operator=(const AGameModeBase&) = delete;
	AGameModeBase(AGameModeBase&&) = delete;
	AGameModeBase& operator=(AGameModeBase&&) = delete;

	/// Stable id matched against level JSON `"gameMode": "<id>"`.
	[[nodiscard]] virtual const char* Id() const = 0;

	/// Prefer `gameModeId == Id()`. Entry name/path may be used as a soft fallback.
	[[nodiscard]] virtual bool Matches(const FLevelEntry& Entry, const std::string& GameModeId) const = 0;

	virtual void OnEnter(UGameEngine& Engine, const std::string& LevelPath) = 0;
	virtual void OnExit(UGameEngine& Engine) = 0;
	virtual void Tick(UGameEngine& Engine, float DeltaTime) = 0;

	/// Unreal `InitGameState` — configure / replace GameState after construction.
	virtual void InitGameState()
	{
	}

	/// Unreal `StartMatch` — authority begins gameplay; notifies GameState.
	virtual void StartMatch()
	{
		GetGameState().HandleMatchHasStarted();
	}
	/// Unreal `EndMatch`.
	virtual void EndMatch()
	{
		GetGameState().HandleMatchHasEnded();
	}

	/// Unreal `AGameModeBase::PostLogin` — registers PlayerState on GameState::PlayerArray.
	virtual void PostLogin(APlayerController& NewPlayer);
	/// Unreal `AGameModeBase::Logout` — removes PlayerState from GameState::PlayerArray.
	virtual void Logout(APlayerController& Exiting);
	/// Unreal `RestartPlayer` — spawn/possess pawn at a FPlayerStart (packs override).
	virtual void RestartPlayer(APlayerController& /*newPlayer*/)
	{
	}
	/// Unreal `HandleStartingNewPlayer` — default calls RestartPlayer.
	virtual void HandleStartingNewPlayer(APlayerController& NewPlayer)
	{
		RestartPlayer(NewPlayer);
	}

	/// Unreal `GetNumPlayers` (from GameState::PlayerArray).
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

	/// Recreate World FPhysScene backend (clears bodies). Prefer before RegisterBodiesFromLevel.
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

	/// Unreal `GetGameState<T>()`.
	template <typename T>
	[[nodiscard]] T* GetGameState()
	{
		static_assert(std::is_base_of_v<AGameStateBase, T>, "T must derive from GameState");
		return dynamic_cast<T*>(GameState.get());
	}
	template <typename T>
	[[nodiscard]] const T* GetGameState() const
	{
		static_assert(std::is_base_of_v<AGameStateBase, T>, "T must derive from GameState");
		return dynamic_cast<const T*>(GameState.get());
	}

	/// Replace the GameState instance (e.g. pack-specific subclass). Calls InitGameState.
	template <typename T, typename... ArgsType>
	T* SetGameState(ArgsType&&... Args)
	{
		static_assert(std::is_base_of_v<AGameStateBase, T>, "T must derive from GameState");
		auto Owned = std::make_unique<T>(std::forward<ArgsType>(Args)...);
		T* Raw = Owned.get();
		GameState = std::move(Owned);
		InitGameState();
		return Raw;
	}

	/// Unreal `UWorld::ServerTravel` / GameMode travel — load map on authority (or local).
	[[nodiscard]] bool ServerTravel(UGameEngine& Engine, std::string_view MapName, std::string_view HintLevelPath = {});
	/// Unreal `APlayerController::ClientTravel` — load map on a joining/remote client.
	[[nodiscard]] bool ClientTravel(UGameEngine& Engine, std::string_view MapName, std::string_view HintLevelPath = {});

	/// Min FPlayerStart Y, or 0 if none.
	[[nodiscard]] static float EstimateFloorY(const ULevel& Level);
	/// Soft XZ walk clamp from static mesh extents (clamped 20–120).
	[[nodiscard]] static float EstimateWalkBounds(const ULevel& Level);

protected:
	AGameModeBase()
		: GameState(std::make_unique<AGameStateBase>())
	{
	}

	/// Framework helper: Level collision meshes → World FPhysScene (not pack business logic).
	void RegisterBodiesFromLevel(const ULevel& Level)
	{
		GetWorld().RegisterBodiesFromLevel(Level);
	}

	/// Unreal `FindPlayerStart` — resolve spawn transform (`slot` picks among starts).
	[[nodiscard]] bool FindPlayerStart(
		const ULevel& Level, glm::vec3& OutLocation, float& OutYawDegrees, int Slot = 0) const
	{
		const auto& Starts = Level.GetPlayerStarts();
		if (Starts.empty())
		{
			OutLocation = {0.0f, 0.0f, 0.0f};
			OutYawDegrees = 0.0f;
			return false;
		}
		const int Index = std::clamp(Slot, 0, static_cast<int>(Starts.size()) - 1);
		const FPlayerStart& Start = Starts[static_cast<std::size_t>(Index)];
		OutLocation = Start.Transform.Position;
		OutYawDegrees = Start.Transform.RotationDegrees.y;
		return true;
	}

	// Flow: Match enter — bodies + nav bake
	void PrepareMatchWorld(
		UGameEngine& Engine, float& OutFloorY, float& OutWalkBounds, EPhysicsBackend Backend = EPhysicsBackend::Jolt);
	void RebuildNavigation(UGameEngine& Engine, float FloorY, float WalkBounds);
	void SnapCharacterToFloor(ACharacter& Character, glm::vec3& InOutFeet, float FloorY) const;

private:
	UWorld World;
	std::unique_ptr<AGameStateBase> GameState;
};
