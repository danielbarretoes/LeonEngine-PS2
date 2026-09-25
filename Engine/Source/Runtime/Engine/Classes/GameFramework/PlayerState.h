#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "PlayerState.generated.h"

/**
 * Per-player session data (UE: APlayerState), an AInfo its controller spawns (AController::InitPlayerState) and owns
 * (GetOwner). The game mode registers it in the game state's PlayerArray (PostLogin).
 */
UCLASS()
class ENGINE_API APlayerState : public AInfo
{
	GENERATED_BODY()

public:
	APlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Clears the score, the lives and the name (UE: Reset). */
	virtual void Reset()
	{
		Score = 0.0f;
		Lives = 0;
		PlayerName.Empty();
	}

	/** UE: GetPlayerId. */
	[[nodiscard]] int32 GetPlayerId() const
	{
		return PlayerId;
	}
	void SetPlayerId(int32 Id)
	{
		PlayerId = Id;
	}

	/** UE: GetPlayerName / SetPlayerName. */
	[[nodiscard]] const FString& GetPlayerName() const
	{
		return PlayerName;
	}
	void SetPlayerName(FString Name)
	{
		PlayerName = MoveTemp(Name);
	}

	/** UE: GetScore / SetScore. */
	[[nodiscard]] float GetScore() const
	{
		return Score;
	}
	void SetScore(float InScore)
	{
		Score = InScore;
	}
	void AddScore(float Delta)
	{
		Score += Delta;
	}

	/** Stocks / lives (Leon). Default 0: games call SetLives at match start. */
	[[nodiscard]] int32 GetLives() const
	{
		return Lives;
	}
	void SetLives(int32 InLives)
	{
		Lives = InLives;
	}
	/** Decrements one life if any remain. Returns true if a life was consumed. */
	[[nodiscard]] bool ConsumeLife()
	{
		if (Lives <= 0)
		{
			return false;
		}
		--Lives;
		return true;
	}

private:
	/** UE: PlayerId. */
	UPROPERTY()
	int32 PlayerId = 0;

	/** UE: Score. */
	UPROPERTY()
	float Score = 0.0f;

	UPROPERTY()
	int32 Lives = 0;

	/** UE: PlayerNamePrivate. */
	UPROPERTY()
	FString PlayerName;
};
