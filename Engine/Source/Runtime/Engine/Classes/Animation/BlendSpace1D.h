#pragma once

#include "Animation/BlendSpaceBase.h"
#include "CoreMinimal.h"
#include "BlendSpace1D.generated.h"

/**
 * A blend space on one axis (UE: UBlendSpace1D): an input such as the speed picks the two nearest samples and a weight
 * between them. The axis is BlendParameters[0], [0, 1] unless set; a sample's position is its SampleValue.X.
 */
UCLASS()
class ENGINE_API UBlendSpace1D : public UBlendSpaceBase
{
	GENERATED_BODY()

public:
	UBlendSpace1D(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	using Super::AddSample;

	/** Places a clip at Position on the axis (Leon). */
	bool AddSample(UAnimSequence* AnimationSequence, float Position)
	{
		return Super::AddSample(AnimationSequence, FVector(Position, 0.0f, 0.0f));
	}

	/**
	 * The samples around BlendInput.X (clamped to the axis): the nearest at or below it and the nearest at or above it,
	 * weighted by the distance (one sample, weight 1, at or past the ends and on a sample). Samples at the same
	 * position keep the order they were added in (the first below, the last above).
	 */
	void GetSamplesFromBlendInput(const FVector& BlendInput, FBlendSampleDataArray& OutSampleDataList) const override;
};
