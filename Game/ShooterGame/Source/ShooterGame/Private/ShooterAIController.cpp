#include "ShooterAIController.h"

AShooterAIController::AShooterAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A bot has a player state like a player (UE: bWantsPlayerState): its team and, from P19, its score and money.
	bWantsPlayerState = true;
}
