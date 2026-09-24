#pragma once

#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

class ACharacter;
class UGameEngine;

/// Drives a possessed Character from player input (Unreal-style APlayerController).
class ENGINE_API APlayerController : public AController
{
public:
	APlayerController()
		: PlayerState(std::make_unique<APlayerState>())
	{
	}

	using AController::Possess;
	void Possess(ACharacter* Character);

	[[nodiscard]] ACharacter* GetCharacter() const
	{
		return AController::GetCharacter();
	}
	[[nodiscard]] bool HasCharacter() const
	{
		return GetCharacter() != nullptr;
	}

	[[nodiscard]] APlayerState& GetPlayerState()
	{
		return *PlayerState;
	}
	[[nodiscard]] const APlayerState& GetPlayerState() const
	{
		return *PlayerState;
	}

	/// Replaces owned PlayerState. Caller must GameMode::Logout (or RemovePlayerState) first
	/// so GameState::PlayerArray does not keep a dangling pointer.
	template <typename T, typename... ArgsType>
	T* SetPlayerState(ArgsType&&... Args)
	{
		static_assert(std::is_base_of_v<APlayerState, T>, "T must derive from PlayerState");
		auto Owned = std::make_unique<T>(std::forward<ArgsType>(Args)...);
		T* Raw = Owned.get();
		PlayerState = std::move(Owned);
		return Raw;
	}

	/// Apply input to the possessed Character. Games override. Returns wish direction for
	/// debug HUD; default is a no-op.
	virtual glm::vec3 TickInput(UGameEngine& Engine);

	/// Unreal-like: drive view from possessed pawn SpringArm (games override).
	virtual void UpdateCamera(UGameEngine& Engine, float DeltaTime);

private:
	std::unique_ptr<APlayerState> PlayerState;
};
