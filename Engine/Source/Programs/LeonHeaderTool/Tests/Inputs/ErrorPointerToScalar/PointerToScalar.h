#pragma once

#include "PointerToScalar.generated.h"

UCLASS()
class UPointerToScalar : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32* Counter;
};
