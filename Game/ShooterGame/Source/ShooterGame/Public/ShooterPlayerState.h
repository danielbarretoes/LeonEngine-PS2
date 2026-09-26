#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ShooterTypes.h"
#include "ShooterPlayerState.generated.h"

/**
 * A player's state in a ShooterGame match (UE ShooterGame: AShooterPlayerState): the team the game mode placed it in
 * and whether it is a bot. P19 adds the money, the kills and the deaths.
 */
UCLASS()
class SHOOTERGAME_API AShooterPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AShooterPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The team (UE ShooterGame: SetTeamNum / GetTeamNum). */
	void SetTeam(EShooterTeam NewTeam)
	{
		Team = NewTeam;
	}
	[[nodiscard]] EShooterTeam GetTeam() const
	{
		return Team;
	}

	/** The player is a bot (UE: APlayerState::bIsABot). */
	UPROPERTY()
	bool bIsABot = false;

private:
	/** UE ShooterGame: TeamNumber. */
	UPROPERTY()
	EShooterTeam Team = EShooterTeam::None;
};
