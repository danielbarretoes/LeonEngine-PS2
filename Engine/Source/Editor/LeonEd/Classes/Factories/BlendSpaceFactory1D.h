#pragma once

#include "CoreMinimal.h"
#include "Factories/BlendSpaceFactoryNew.h"
#include "BlendSpaceFactory1D.generated.h"

/** Makes a UBlendSpace1D from its description (UE: UBlendSpaceFactory1D); see UBlendSpaceFactoryNew. */
UCLASS()
class LEONED_API UBlendSpaceFactory1D : public UBlendSpaceFactoryNew
{
	GENERATED_BODY()

public:
	UBlendSpaceFactory1D(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	int32 GetNumAxes() const override
	{
		return 1;
	}
};
