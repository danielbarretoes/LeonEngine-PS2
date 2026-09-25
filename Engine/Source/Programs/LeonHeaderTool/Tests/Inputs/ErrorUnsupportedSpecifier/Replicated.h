#pragma once

#include "Replicated.generated.h"

UCLASS()
class UReplicated : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Replicated)
	int32 Health;
};
