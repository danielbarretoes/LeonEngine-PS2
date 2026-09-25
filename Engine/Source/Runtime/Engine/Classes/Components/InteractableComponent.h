#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "InteractableComponent.generated.h"

/**
 * What a player can do at an actor, usually an ATriggerVolume (Leon; UE has no counterpart: a game gives its volumes
 * its own component or subclass). The engine only stores it and reads it in VolumeHelpers (FindBestTriggerVolume,
 * FormatDefaultInteractPrompt); the payload's meaning is the game's (plan decision D15 keeps game meaning out of the
 * engine's classes).
 *
 * The legacy levels' trigger volumes carry their interaction data in one (P15).
 */
UCLASS()
class ENGINE_API UInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractableComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** How close a player's feet must be, in the horizontal plane, cm. */
	UPROPERTY()
	float InteractRadius = 200.0f;

	/** What using it costs the player (the game's currency; 0 = free). */
	UPROPERTY()
	int32 InteractCost = 0;

	/** Game-defined text (`Door`, `WallBuy:<Weapon>`, `Perk:<Name>`, ...). */
	UPROPERTY()
	FString Payload;

	/** Used once. */
	UPROPERTY()
	bool bConsumeOnUse = false;
};
