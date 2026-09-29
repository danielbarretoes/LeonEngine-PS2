#pragma once

#include "Animation/AnimationAsset.h"
#include "AnimationRuntime.h"
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

	/** Where the clip sits on the axes (X for a 1D space; X and Y for a 2D one) (UE: SampleValue). */
	UPROPERTY()
	FVector SampleValue = FVector::ZeroVector;

	/** UE: RateScale (Leon plays every sample at the blend's synchronized rate). */
	UPROPERTY()
	float RateScale = 1.0f;
};

/** The samples of a blend and their weights: at most three (a 2D space's triangle). */
typedef TArray<FBlendSampleData, TInlineAllocator<3>> FBlendSampleDataArray;

/**
 * The base of the blend spaces (UE: UBlendSpaceBase): clips placed on up to three axes. An input picks some samples
 * and their weights (GetSamplesFromBlendInput); the player (UAnimInstance) plays them synchronized: one normalized time
 * for all, each at its own length, advanced by the weighted length (UE's length-based sync).
 */
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

	/** Places a clip and rebuilds the interpolation data; false for a null clip (UE: AddSample, editor-only there). */
	bool AddSample(UAnimSequence* AnimationSequence, const FVector& SampleValue);

	/** Removes every sample (Leon). */
	void ClearSamples()
	{
		SampleData.Reset();
		ResampleData();
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

	/**
	 * The samples an input blends and their weights, which add up to 1 (UE: GetSamplesFromBlendInput): the input is
	 * clamped to the axes first. Samples without a clip with frames count as the others do; the player falls back to
	 * the reference pose for them. Empty without samples. Allocates nothing.
	 */
	virtual void GetSamplesFromBlendInput(const FVector& /*BlendInput*/, FBlendSampleDataArray& OutSampleDataList) const
	{
		OutSampleDataList.Reset();
	}

	/** The weighted length of the samples of a blend, seconds (UE: GetAnimationLengthFromSampleData). */
	[[nodiscard]] float GetAnimationLengthFromSampleData(const FBlendSampleDataArray& SampleDataList) const;

	/** The sample with the largest weight, or INDEX_NONE (UE: the highest weighted animation, whose notifies fire). */
	[[nodiscard]] static int32 GetHighestWeightedSample(const FBlendSampleDataArray& SampleDataList);

	/** Rebuilds what the interpolation needs from the samples (UE: ResampleData); the base needs nothing. */
	virtual void ResampleData()
	{
	}

	/** Loaded: the interpolation data is rebuilt (it is not saved). */
	void PostLoad() override;

protected:
	/** The input clamped to the axes' ranges. */
	[[nodiscard]] FVector ClampBlendInput(const FVector& BlendInput) const;
};
