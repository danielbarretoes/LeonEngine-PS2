#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "PlayerStartPIE.generated.h"

/**
 * A "Play From Here" start (UE: APlayerStartPIE, which the editor spawns at the viewport camera): the game mode picks
 * it before any other start (AGameModeBase::ChoosePlayerStart), and the player's control rotation takes its whole
 * rotation.
 *
 * Leon: UEngine::LoadMap spawns one, transient, at the view a `.llev` level's camera framing opens with, so the default
 * pawn starts where the legacy engine camera did. It goes away with the `.llev` format (P15).
 */
UCLASS(NotPlaceable, Transient)
class ENGINE_API APlayerStartPIE : public APlayerStart
{
	GENERATED_BODY()

public:
	APlayerStartPIE(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
