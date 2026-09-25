#pragma once

#include "Unrecognized.generated.h"

UCLASS()
class UUnrecognized : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Fine;

	UPROPERTY()
	FVector2D Position;
};
