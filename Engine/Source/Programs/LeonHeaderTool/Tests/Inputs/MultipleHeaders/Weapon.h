// Declares the types Inventory.h uses.
#pragma once

#include "CoreMinimal.h"
#include "Weapon.generated.h"

UENUM()
enum class EWeaponSlot : uint8
{
	Primary,
	Secondary
};

USTRUCT()
struct FAmmo
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Rounds = 0;
};

UCLASS(Abstract)
class UWeapon : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class URifle : public UWeapon
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FAmmo Magazine;
};
