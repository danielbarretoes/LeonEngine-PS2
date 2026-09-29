#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SaveGame.generated.h"

/**
 * The base of what a game saves (UE: USaveGame): a subclass's UPROPERTYs are its data. UGameplayStatics makes one
 * (CreateSaveGameObject), writes it to a slot (SaveGameToSlot: Saved/SaveGames/ on the desktop, the memory card on the
 * PS2, ISaveGameSystem) and reads it back (LoadGameFromSlot); Docs/PLANS/ps2-shipping.md N24.
 */
UCLASS(Abstract)
class ENGINE_API USaveGame : public UObject
{
	GENERATED_BODY()

public:
	USaveGame(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
