#pragma once

#include "Animation/AnimationAsset.h"
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BlendSpaceBase.generated.h"

class UAnimSequence;

/** One axis of a blend space (UE: FBlendParameter). */
USTRUCT()
struct ENGINE_API FBlendParameter
{
	GENERATED_BODY()

	UPROPERTY()
	FString DisplayName = TEXT("None");

	/** The axis range; an input outside it is clamped (UE: Min / Max). */
	UPROPERTY()
	float Min = 0.0f;

	UPROPERTY()
	float Max = 100.0f;

	/** Grid divisions (UE: GridNum; Leon does not snap samples to it). */
	UPROPERTY()
	int32 GridNum = 4;
};

/** A clip placed in a blend space (UE: FBlendSample). */
USTRUCT()
struct ENGINE_API FBlendSample
{
	GENERATED_BODY()

	FBlendSample() = default;
	FBlendSample(UAnimSequence* InAnimation, const FVector& InSampleValue)
		: Animation(InAnimation)
		, SampleValue(InSampleValue)
	{
	}

	/** UE: Animation. */
	UPROPERTY()
	UAnimSequence* Animation = nullptr;

	/** Where the clip sits on the axes (X for a 1D space) (UE: SampleValue). */
	UPROPERTY()
	FVector SampleValue = FVector::ZeroVector;

	/** UE: RateScale (Leon plays every sample at the anim instance's rate). */
	UPROPERTY()
	float RateScale = 1.0f;
};

/** The base of the blend spaces (UE: UBlendSpaceBase): clips placed on up to three axes. */
UCLASS(Abstract)
class ENGINE_API UBlendSpaceBase : public UAnimationAsset
{
	GENERATED_BODY()

public:
	UBlendSpaceBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The axes (UE: BlendParameters). */
	UPROPERTY()
	FBlendParameter BlendParameters[3];

	/** The placed clips (UE: SampleData). */
	UPROPERTY()
	TArray<FBlendSample> SampleData;

	/** Places a clip; false for a null clip (UE: AddSample, editor-only there). */
	bool AddSample(UAnimSequence* AnimationSequence, const FVector& SampleValue);

	/** Removes every sample (Leon). */
	void ClearSamples()
	{
		SampleData.Reset();
	}

	/** UE: GetBlendSamples. */
	[[nodiscard]] const TArray<FBlendSample>& GetBlendSamples() const
	{
		return SampleData;
	}

	/** UE: GetBlendParameter. */
	[[nodiscard]] const FBlendParameter& GetBlendParameter(int32 Index) const
	{
		check(Index >= 0 && Index < 3);
		return BlendParameters[Index];
	}
};
