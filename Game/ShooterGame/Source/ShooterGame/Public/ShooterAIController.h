#pragma once

#include "AIController.h"
#include "CoreMinimal.h"
#include "ShooterAIController.generated.h"

/**
 * A bot's controller (UE ShooterGame: AShooterAIController). P17's bots have no brain yet: the controller owns the
 * bot's player state (its team) and its pawn, which stands at its team's start. P20 gives it the behaviour tree,
 * the senses and the waypoint navigation.
 */
UCLASS()
class SHOOTERGAME_API AShooterAIController : public AAIController
{
	GENERATED_BODY()

public:
	AShooterAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
