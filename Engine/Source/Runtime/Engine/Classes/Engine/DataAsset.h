#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DataAsset.generated.h"

/**
 * The base of the plain data assets (UE: UDataAsset): a game derives a UCLASS with its UPROPERTYs, makes instances and
 * saves them in `.lasset` packages; the tagged properties are the whole format. Leon has no asset manager, so there is
 * no UPrimaryDataAsset yet.
 */
UCLASS(Abstract)
class ENGINE_API UDataAsset : public UObject
{
	GENERATED_BODY()
};
