#pragma once

#include "Malformed.generated.h"

UCLASS()
class UMalformed : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere Category = "Stats")
	int32 Value;
};
