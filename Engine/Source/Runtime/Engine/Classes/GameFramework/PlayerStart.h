#pragma once

#include "Components/CapsuleComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerStart.generated.h"

/**
 * Where players spawn (UE: APlayerStart). The game mode's FindPlayerStart picks one; its location and yaw place the
 * pawn. PlayerStartTag carries game meaning (plan decision D15: `CT` / `T` for the teams of a map), so the engine never
 * knows the game.
 *
 * UE derives it from ANavigationObjectBase, which owns the capsule; Leon has no navigation objects and gives the
 * capsule to the player start itself. The capsule only shows the spawn size: it has no collision.
 */
UCLASS()
class ENGINE_API APlayerStart : public AActor
{
	GENERATED_BODY()

public:
	APlayerStart(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Game meaning of this start (UE: PlayerStartTag): the game mode chooses starts by it. */
	UPROPERTY()
	FName PlayerStartTag;

	/** UE: GetCapsuleComponent. */
	[[nodiscard]] UCapsuleComponent* GetCapsuleComponent() const
	{
		return CapsuleComponent;
	}

private:
	/** The root (UE: ANavigationObjectBase::CapsuleComponent, "CollisionCapsule"). */
	UPROPERTY()
	UCapsuleComponent* CapsuleComponent = nullptr;
};
