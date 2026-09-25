// Enum classes with underlying types, regular enums, UMETA, enumerator values and enum properties.
#pragma once

#include "CoreMinimal.h"
#include "EnumsObject.generated.h"

UENUM(BlueprintType)
enum class ETeam : uint8
{
	None UMETA(Hidden),
	CounterTerrorists = 1 UMETA(DisplayName = "CT"),
	Terrorists = (1 << 1),
};

UENUM(Flags)
enum class EWeaponFlags : int32
{
	None = 0,
	Automatic = 0x01,
	Silenced = 0x02
};

UENUM()
enum class EUntyped
{
	First,
	Second
};

UENUM()
enum ERoundState
{
	RS_Freeze,
	RS_Live = 5,
	RS_End
};

UCLASS()
class UEnumsObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	ETeam Team = ETeam::None;

	UPROPERTY()
	EWeaponFlags WeaponFlags;

	UPROPERTY()
	TEnumAsByte<ERoundState> RoundState;

	UPROPERTY()
	TMap<ETeam, int32> Scores;
};
