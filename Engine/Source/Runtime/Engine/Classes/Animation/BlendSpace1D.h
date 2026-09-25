#pragma once

#include "Animation/BlendSpaceBase.h"
#include "CoreMinimal.h"
#include "BlendSpace1D.generated.h"

/**
 * A blend space on one axis (UE: UBlendSpace1D): an input such as the speed picks the two nearest clips and a weight
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
	 * The two clips around AxisValue (clamped to the axis) and the weight toward the second: OutA == OutB with a weight
	 * of 0 at or past the ends and on a sample. Samples at the same position keep the order they were added in. Null
	 * clips without samples (Leon; UE evaluates the samples' weights through its grid).
	 */
	void Evaluate(float AxisValue, const UAnimSequence*& OutA, const UAnimSequence*& OutB, float& OutAlpha) const;
};
