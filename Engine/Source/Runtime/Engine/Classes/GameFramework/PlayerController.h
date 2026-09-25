#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"

class ACharacter;
class UGameEngine;

/** Drives a possessed Character from player input (Unreal-style APlayerController). */
class ENGINE_API APlayerController : public AController
{
public:
	APlayerController()
		: PlayerState(MakeUnique<APlayerState>())
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

	/**
	 * Replaces owned PlayerState. Caller must GameMode::Logout (or RemovePlayerState) first
	 * so GameState::PlayerArray does not keep a dangling pointer.
	 */
	template <typename T, typename... ArgsType>
	T* SetPlayerState(ArgsType&&... Args)
	{
		static_assert(TIsDerivedFrom<T, APlayerState>::Value, "T must derive from PlayerState");
		auto Owned = MakeUnique<T>(Forward<ArgsType>(Args)...);
		T* Raw = Owned.get();
		PlayerState = MoveTemp(Owned);
		return Raw;
	}

	/**
	 * Apply input to the possessed Character. Games override. Returns wish direction for
	 * debug HUD; default is a no-op.
	 */
	virtual FVector TickInput(UGameEngine& Engine);

	/** Unreal-like: drive view from possessed pawn SpringArm (games override). */
	virtual void UpdateCamera(UGameEngine& Engine, float DeltaTime);

private:
	TUniquePtr<APlayerState> PlayerState;
};
