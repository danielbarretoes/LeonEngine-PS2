#pragma once

#include "OutParam.generated.h"

UCLASS()
class UOutParam : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void GetValues(int32 Count,
		TArray<int32>& OutValues);
};
