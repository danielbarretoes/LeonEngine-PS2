#include "GameFramework/PlayerState.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

APlayerState::APlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void APlayerState::Destroyed()
{
	if (UWorld* World = GetWorld())
	{
		if (AGameStateBase* GameState = World->GetGameState())
		{
			GameState->RemovePlayerState(this);
		}
	}
	Super::Destroyed();
}
