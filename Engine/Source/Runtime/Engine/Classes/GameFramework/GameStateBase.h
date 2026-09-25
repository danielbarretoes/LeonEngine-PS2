#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "GameStateBase.generated.h"

class APlayerState;

/**
 * Shared match/session state (UE: AGameStateBase), an AInfo the game mode spawns (GameStateClass) and the world
 * points at (UWorld::GetGameState).
 *
 * Leon keeps a simple match clock here: HandleMatchHasStarted / HandleMatchHasEnded and a clock that Tick advances
 * while the match is in progress (the game mode ticks its game state; AInfo actors do not tick in the world). Begin
 * play starts it (HandleBeginPlay). AGameState adds UE's MatchState.
 */
UCLASS()
class ENGINE_API AGameStateBase : public AInfo
{
	GENERATED_BODY()

public:
	AGameStateBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Resets match clock / flags / map — not PlayerArray (Unreal: logout removes players). */
	virtual void Reset()
	{
		ElapsedSeconds = 0.0f;
		bMatchInProgress = false;
		bMatchHasEnded = false;
		ReplicatedWorldTimeFrames = 0;
		MapName.Empty();
	}

	void Tick(float DeltaTime) override
	{
		Super::Tick(DeltaTime);
		if (bMatchInProgress && !bMatchHasEnded)
		{
			ElapsedSeconds += DeltaTime;
		}
	}

	/** Unreal HasMatchStarted. */
	[[nodiscard]] bool HasMatchStarted() const
	{
		return bMatchInProgress;
	}
	/** Unreal HasMatchEnded. */
	[[nodiscard]] bool HasMatchEnded() const
	{
		return bMatchHasEnded;
	}
	/** Unreal GetServerWorldTimeSeconds (local elapsed while match is in progress). */
	[[nodiscard]] float GetServerWorldTimeSeconds() const
	{
		return ElapsedSeconds;
	}

	/** Unreal PlayerArray — PlayerStates registered via PostLogin / Logout. */
	[[nodiscard]] const TArray<APlayerState*>& GetPlayerArray() const
	{
		return PlayerArray;
	}
	/** Unreal PlayerArray.Num(). */
	[[nodiscard]] int32 GetNumPlayers() const
	{
		return PlayerArray.Num();
	}

	/** Unreal AGameStateBase::AddPlayerState (idempotent). */
	void AddPlayerState(APlayerState* PlayerState)
	{
		if (PlayerState == nullptr)
		{
			return;
		}
		PlayerArray.AddUnique(PlayerState);
	}

	/** Unreal AGameStateBase::RemovePlayerState. */
	void RemovePlayerState(APlayerState* PlayerState)
	{
		if (PlayerState == nullptr)
		{
			return;
		}
		PlayerArray.Remove(PlayerState);
	}

	[[nodiscard]] bool HasPlayerState(const APlayerState* PlayerState) const
	{
		if (PlayerState == nullptr)
		{
			return false;
		}
		return PlayerArray.ContainsByPredicate(
			[PlayerState](const APlayerState* Entry) { return Entry == PlayerState; });
	}

	/**
	 * The world began play (UE: HandleBeginPlay, from AGameModeBase::StartPlay). Without match states the match is play
	 * itself, so Leon's clock starts (HandleMatchHasStarted); AGameState starts it when its match is in progress.
	 */
	virtual void HandleBeginPlay()
	{
		MarkHasBegunPlay();
		HandleMatchHasStarted();
	}

	/** True once the world began play (UE: HasBegunPlay). */
	[[nodiscard]] bool HasBegunPlay() const
	{
		return bReplicatedHasBegunPlay;
	}

	/** Authority: mark match in progress (pairs with GameMode::StartMatch). */
	virtual void HandleMatchHasStarted()
	{
		bMatchInProgress = true;
		bMatchHasEnded = false;
	}
	/** Authority: mark match finished (pairs with GameMode::EndMatch). */
	virtual void HandleMatchHasEnded()
	{
		bMatchInProgress = false;
		bMatchHasEnded = true;
	}

	/** Current map identity (Level document name / travel key). Unreal: map package name. */
	[[nodiscard]] const FString& GetMapName() const
	{
		return MapName;
	}
	void SetMapName(FString Name)
	{
		MapName = MoveTemp(Name);
	}

	/** Replicated simulation frame (host advances; clients apply from Snapshot). */
	[[nodiscard]] uint32 GetReplicatedWorldTimeFrames() const
	{
		return ReplicatedWorldTimeFrames;
	}
	void SetReplicatedWorldTimeFrames(uint32 InTick)
	{
		ReplicatedWorldTimeFrames = InTick;
	}
	void IncrementReplicatedWorldTimeFrames()
	{
		++ReplicatedWorldTimeFrames;
	}

protected:
	/** Records that the world began play, without starting the clock (AGameState). */
	void MarkHasBegunPlay()
	{
		bReplicatedHasBegunPlay = true;
	}

private:
	/** Seconds the match has been in progress (Leon; UE: GetServerWorldTimeSeconds of a replicated world time). */
	UPROPERTY()
	float ElapsedSeconds = 0.0f;

	UPROPERTY()
	bool bMatchInProgress = false;

	/** The world began play (UE: bReplicatedHasBegunPlay). */
	UPROPERTY()
	bool bReplicatedHasBegunPlay = false;

	UPROPERTY()
	bool bMatchHasEnded = false;

	UPROPERTY()
	uint32 ReplicatedWorldTimeFrames = 0;

	UPROPERTY()
	FString MapName;

	/** The players' states (UE: PlayerArray). */
	UPROPERTY()
	TArray<APlayerState*> PlayerArray;
};
