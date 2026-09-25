#pragma once

#include "StaticArray.generated.h"

USTRUCT()
struct FStaticArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> Lists[3];
};
