#pragma once

#include "CoreMinimal.h"
#include "Factories/BlendSpaceFactory1D.h"
#include "AimOffsetBlendSpaceFactory1D.generated.h"

/**
 * Makes a UAimOffsetBlendSpace1D from its description (UE: UAimOffsetBlendSpaceFactory1D): the aim poses by pitch
 * (`AxisX=Pitch,-90,90`, `+Sample=A_Aim_Up,90`...) and the BasePose they are measured from.
 */
UCLASS()
class LEONED_API UAimOffsetBlendSpaceFactory1D : public UBlendSpaceFactory1D
{
	GENERATED_BODY()

public:
	UAimOffsetBlendSpaceFactory1D(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
