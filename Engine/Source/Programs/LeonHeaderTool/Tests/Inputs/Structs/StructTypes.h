// USTRUCTs: inheritance, GENERATED_USTRUCT_BODY, struct members, bitfields, an empty struct.
#pragma once

#include "CoreMinimal.h"
#include "StructTypes.generated.h"

USTRUCT(BlueprintType)
struct LHTTEST_API FBaseStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 Level = 1;

	bool operator==(const FBaseStats& Other) const
	{
		return Level == Other.Level;
	}
};

USTRUCT()
struct FDerivedStats : public FBaseStats
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FBaseStats Nested;

	UPROPERTY()
	TMap<FName, FBaseStats> ByName;

	UPROPERTY()
	uint32 bHidden : 1;

private:
	UPROPERTY()
	FString Secret;
};

USTRUCT(Atomic)
struct FEmptyStruct
{
	GENERATED_BODY()
};

UCLASS()
class UStructHolder : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FDerivedStats Stats;
};
