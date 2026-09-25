#pragma once

#include "RawEnum.generated.h"

UENUM()
enum ERawEnum
{
	RE_A,
	RE_B
};

UCLASS()
class URawEnum : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	ERawEnum Value;
};
