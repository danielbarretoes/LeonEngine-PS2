#include "Animation/BlendSpace.h"

#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"

UBlendSpaceBase::UBlendSpaceBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBlendSpaceBase::AddSample(UAnimSequence* AnimationSequence, const FVector& SampleValue)
{
	if (AnimationSequence == nullptr)
	{
		return false;
	}
	SampleData.Add(FBlendSample(AnimationSequence, SampleValue));
	ResampleData();
	return true;
}

void UBlendSpaceBase::PostLoad()
{
	Super::PostLoad();
	ResampleData();
}

FVector UBlendSpaceBase::ClampBlendInput(const FVector& BlendInput) const
{
	FVector Clamped = BlendInput;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const FBlendParameter& Parameter = BlendParameters[Axis];
		Clamped[Axis] = FMath::Clamp(
			Clamped[Axis], FMath::Min(Parameter.Min, Parameter.Max), FMath::Max(Parameter.Min, Parameter.Max));
	}
	return Clamped;
}

float UBlendSpaceBase::GetAnimationLengthFromSampleData(const FBlendSampleDataArray& SampleDataList) const
{
	float Length = 0.0f;
	for (const FBlendSampleData& Sample : SampleDataList)
	{
		const UAnimSequence* Animation =
			SampleData.IsValidIndex(Sample.SampleDataIndex) ? SampleData[Sample.SampleDataIndex].Animation : nullptr;
		if (Animation != nullptr)
		{
			Length += Animation->GetPlayLength() * Sample.TotalWeight;
		}
	}
	return Length;
}

int32 UBlendSpaceBase::GetHighestWeightedSample(const FBlendSampleDataArray& SampleDataList)
{
	int32 Best = INDEX_NONE;
	float BestWeight = -1.0f;
	for (const FBlendSampleData& Sample : SampleDataList)
	{
		if (Sample.TotalWeight > BestWeight)
		{
			BestWeight = Sample.TotalWeight;
			Best = Sample.SampleDataIndex;
		}
	}
	return Best;
}

UBlendSpace1D::UBlendSpace1D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Leon's locomotion axis is a normalized speed.
	BlendParameters[0].Min = 0.0f;
	BlendParameters[0].Max = 1.0f;
}

void UBlendSpace1D::GetSamplesFromBlendInput(const FVector& BlendInput, FBlendSampleDataArray& OutSampleDataList) const
{
	OutSampleDataList.Reset();
	if (SampleData.Num() == 0)
	{
		return;
	}
	const float X = ClampBlendInput(BlendInput).X;
	// The nearest sample at or below X (the first of equal positions) and at or above it (the last of equal ones).
	int32 Below = INDEX_NONE;
	int32 Above = INDEX_NONE;
	for (int32 Index = 0; Index < SampleData.Num(); ++Index)
	{
		const float Position = SampleData[Index].SampleValue.X;
		if (Position <= X && (Below == INDEX_NONE || Position > SampleData[Below].SampleValue.X))
		{
			Below = Index;
		}
		if (Position >= X && (Above == INDEX_NONE || Position <= SampleData[Above].SampleValue.X))
		{
			Above = Index;
		}
	}
	if (Below == INDEX_NONE || Above == INDEX_NONE || Below == Above)
	{
		OutSampleDataList.Add({Below != INDEX_NONE ? Below : Above, 1.0f});
		return;
	}
	const float From = SampleData[Below].SampleValue.X;
	const float Span = SampleData[Above].SampleValue.X - From;
	const float Alpha = Span > 1.0e-6f ? (X - From) / Span : 0.0f;
	if (Alpha <= 1.0e-5f)
	{
		OutSampleDataList.Add({Below, 1.0f});
		return;
	}
	if (Alpha >= 1.0f - 1.0e-5f)
	{
		OutSampleDataList.Add({Above, 1.0f});
		return;
	}
	OutSampleDataList.Add({Below, 1.0f - Alpha});
	OutSampleDataList.Add({Above, Alpha});
}

UBlendSpace::UBlendSpace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FVector2D UBlendSpace::NormalizeInput(const FVector& Value) const
{
	FVector2D Normalized;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const FBlendParameter& Parameter = BlendParameters[Axis];
		const float Range = Parameter.Max - Parameter.Min;
		Normalized[Axis] = FMath::Abs(Range) > 1.0e-6f ? (Value[Axis] - Parameter.Min) / Range : 0.0f;
	}
	return Normalized;
}

void UBlendSpace::ResampleData()
{
	NormalizedSamples.Reset();
	for (const FBlendSample& Sample : SampleData)
	{
		NormalizedSamples.Add(NormalizeInput(Sample.SampleValue));
	}
	FBlendSpaceTriangulation::Triangulate(NormalizedSamples, Triangles);
}

void UBlendSpace::GetSamplesFromBlendInput(const FVector& BlendInput, FBlendSampleDataArray& OutSampleDataList) const
{
	OutSampleDataList.Reset();
	if (NormalizedSamples.Num() != SampleData.Num() || SampleData.Num() == 0)
	{
		return;
	}
	FBlendSampleData Samples[3];
	const int32 Count = FBlendSpaceTriangulation::GetWeights(
		NormalizedSamples, Triangles, NormalizeInput(ClampBlendInput(BlendInput)), Samples);
	// In the order of the samples, so a blend is the same whichever corner its triangle starts at.
	for (int32 Index = 0; Index < Count; ++Index)
	{
		int32 Insert = OutSampleDataList.Num();
		while (Insert > 0 && OutSampleDataList[Insert - 1].SampleDataIndex > Samples[Index].SampleDataIndex)
		{
			--Insert;
		}
		OutSampleDataList.Insert(Samples[Index], Insert);
	}
}

UAimOffsetBlendSpace1D::UAimOffsetBlendSpace1D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Pitch in degrees (UE's aim offsets use -90 to 90 too).
	BlendParameters[0].DisplayName = TEXT("Pitch");
	BlendParameters[0].Min = -90.0f;
	BlendParameters[0].Max = 90.0f;
}

const UAnimSequence* UAimOffsetBlendSpace1D::GetAdditiveBasePose() const
{
	if (BasePose != nullptr)
	{
		return BasePose;
	}
	const FBlendSample* Nearest = nullptr;
	for (const FBlendSample& Sample : SampleData)
	{
		if (Nearest == nullptr || FMath::Abs(Sample.SampleValue.X) < FMath::Abs(Nearest->SampleValue.X))
		{
			Nearest = &Sample;
		}
	}
	return Nearest != nullptr ? Nearest->Animation : nullptr;
}
