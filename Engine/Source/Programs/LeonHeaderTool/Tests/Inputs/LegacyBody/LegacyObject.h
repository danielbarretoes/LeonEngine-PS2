// GENERATED_UCLASS_BODY (legacy constructors), a multi-line UCLASS and a user FVTableHelper constructor.
#pragma once

#include "CoreMinimal.h"
#include "LegacyObject.generated.h"

UCLASS(
	Transient,
	BlueprintType)
class ULegacyObject : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	int32 Value;
};

UCLASS(MinimalAPI, NotPlaceable, EditInlineNew, DefaultConfig, Config = Engine)
class UCustomVTableObject : public UObject
{
	GENERATED_BODY()

public:
	UCustomVTableObject(FVTableHelper& Helper);
	UCustomVTableObject(const FObjectInitializer& ObjectInitializer);
};
