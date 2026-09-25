#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "CookTestTypes.generated.h"

/** An asset with a hard and a soft reference: the cook's dependency closure follows both (CookTests.cpp). */
UCLASS()
class UCookTestAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Saved as an import: the cook needs its package. */
	UPROPERTY()
	UObject* HardRef = nullptr;

	/** Saved as a soft package reference: the cook takes its package too, and only warns when it is missing. */
	UPROPERTY()
	TSoftObjectPtr<UObject> SoftRef;
};
