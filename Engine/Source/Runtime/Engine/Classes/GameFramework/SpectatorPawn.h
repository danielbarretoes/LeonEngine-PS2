#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "SpectatorPawn.generated.h"

class USpectatorPawnMovement;

/**
 * The pawn of a player who watches: a dead player, or one who joined as a spectator (UE: ASpectatorPawn). A default
 * pawn that flies free (USpectatorPawnMovement, no collision) and takes no damage. APlayerController spawns it in the
 * spectating state (ChangeState(NAME_Spectating) → SpawnSpectatorPawn, of the game mode's SpectatorClass).
 *
 * Leon: the controller possesses its spectator pawn (UE keeps it apart from the possessed pawn, GetPawnOrSpectator),
 * so it moves and looks with the player's input as a default pawn does; the pawn does not show the default pawn's
 * on-screen hint.
 */
UCLASS()
class ENGINE_API ASpectatorPawn : public ADefaultPawn
{
	GENERATED_BODY()

public:
	ASpectatorPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The default pawn's move and look axes (UE: ADefaultPawn's bindings), without its hint. */
	void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** The spectator's movement (UE: GetSpectatorPawnMovement). */
	[[nodiscard]] USpectatorPawnMovement* GetSpectatorPawnMovement() const;
};
