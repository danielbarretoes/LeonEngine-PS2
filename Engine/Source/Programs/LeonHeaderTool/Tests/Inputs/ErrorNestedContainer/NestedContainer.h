#pragma once

#include "NestedContainer.generated.h"

UCLASS()
class UNestedContainer : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<TArray<int32>> Grid;
};
