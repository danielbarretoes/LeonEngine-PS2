// TArray / TMap / TSet of every supported inner kind, nested templates and C arrays.
#pragma once

#include "CoreMinimal.h"
#include "ContainersObject.generated.h"

UENUM()
enum class EContainerKind : uint8
{
	Small,
	Large
};

USTRUCT()
struct FContainerEntry
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;
};

UCLASS()
class UContainersObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TArray<int32> Values;

	UPROPERTY()
	TArray<FString> Names;

	UPROPERTY()
	TArray<UObject*> Objects;

	UPROPERTY()
	TArray<TSubclassOf<UObject>> Classes;

	UPROPERTY()
	TMap<FName, int32> Counts;

	UPROPERTY()
	TMap<FString, TSoftObjectPtr<UObject>> Assets;

	UPROPERTY()
	TSet<FName> Tags;

	UPROPERTY()
	TArray<bool> Flags;

	UPROPERTY()
	TArray<EContainerKind> Kinds;

	UPROPERTY()
	TArray<FContainerEntry> Entries = {};

	UPROPERTY(EditFixedSize)
	float Weights[4];

	UPROPERTY()
	FName Slots[2] = {};
};
