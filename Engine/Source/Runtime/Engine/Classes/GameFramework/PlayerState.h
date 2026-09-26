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

	/** Leaves the game state's PlayerArray (UE). */
	void Destroyed() override;

	/** Clears the score and the name (UE: Reset). */
	virtual void Reset()
	{
		Score = 0.0f;
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

private:
	/** UE: PlayerId. */
	UPROPERTY()
	int32 PlayerId = 0;

	/** UE: Score. */
	UPROPERTY()
	float Score = 0.0f;

	/** UE: PlayerNamePrivate. */
	UPROPERTY()
	FString PlayerName;
};
