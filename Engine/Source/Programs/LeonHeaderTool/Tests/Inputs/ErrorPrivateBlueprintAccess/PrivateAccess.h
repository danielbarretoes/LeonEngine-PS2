#pragma once

#include "PrivateAccess.generated.h"

UCLASS()
class UPrivateAccess : public UObject
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 PrivateButFine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PrivateOffset;
};
