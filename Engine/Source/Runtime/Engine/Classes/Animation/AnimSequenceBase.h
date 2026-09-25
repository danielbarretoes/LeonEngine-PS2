#pragma once

#include "Animation/AnimationAsset.h"
#include "CoreMinimal.h"
#include "AnimSequenceBase.generated.h"

/** The base of the assets that play over time (UE: UAnimSequenceBase). */
UCLASS(Abstract)
class ENGINE_API UAnimSequenceBase : public UAnimationAsset
{
	GENERATED_BODY()

public:
	UAnimSequenceBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Length in seconds at rate 1 (UE: SequenceLength). */
	UPROPERTY()
	float SequenceLength = 1.0f;

	/** Speed multiplier (UE: RateScale; Leon's anim instances pass their own play rates). */
	UPROPERTY()
	float RateScale = 1.0f;

	/** Wraps around at the end; a one-shot holds its last frame (UE: bLoop). */
	UPROPERTY()
	bool bLoop = true;

	/** UE: GetPlayLength. */
	[[nodiscard]] virtual float GetPlayLength() const
	{
		return SequenceLength;
	}

	/** Sampled frames (UE: GetNumberOfFrames); none in the base. */
	[[nodiscard]] virtual int32 GetNumberOfFrames() const
	{
		return 0;
	}
};
