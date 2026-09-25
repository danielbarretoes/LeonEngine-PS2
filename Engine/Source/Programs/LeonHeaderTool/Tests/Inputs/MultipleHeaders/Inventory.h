// Uses types declared in Weapon.h, which comes later in the header list.
#pragma once

#include "CoreMinimal.h"
#include "Weapon.h"
#include "Inventory.generated.h"

UCLASS()
class UInventory : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<UWeapon*> Weapons;

	UPROPERTY()
	FAmmo Reserve;

	UPROPERTY()
	EWeaponSlot ActiveSlot;
};
