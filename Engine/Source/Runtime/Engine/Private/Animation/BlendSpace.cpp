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
	return true;
}

UBlendSpace1D::UBlendSpace1D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Leon's locomotion axis is a normalized speed.
	BlendParameters[0].Min = 0.0f;
	BlendParameters[0].Max = 1.0f;
}

void UBlendSpace1D::Evaluate(
	float AxisValue, const UAnimSequence*& OutA, const UAnimSequence*& OutB, float& OutAlpha) const
{
	OutA = nullptr;
	OutB = nullptr;
	OutAlpha = 0.0f;
	if (SampleData.Num() == 0)
	{
		return;
	}

	// Sample indices sorted by position (stable, so equal positions keep their authoring order).
	TArray<int32> Order;
	Order.SetNum(SampleData.Num());
	for (int32 I = 0; I < SampleData.Num(); ++I)
	{
		Order[I] = I;
	}
	StableSort(Order.GetData(), Order.Num(),
		[this](int32 A, int32 B) { return SampleData[A].SampleValue.X < SampleData[B].SampleValue.X; });

	const FBlendParameter& Axis = BlendParameters[0];
	const float X = FMath::Clamp(AxisValue, Axis.Min, Axis.Max);
	const FBlendSample& First = SampleData[Order[0]];
	const FBlendSample& Last = SampleData[Order.Last()];
	if (X <= First.SampleValue.X || Order.Num() == 1)
	{
		OutA = First.Animation;
		OutB = First.Animation;
		OutAlpha = 0.0f;
		return;
	}
	if (X >= Last.SampleValue.X)
	{
		OutA = Last.Animation;
		OutB = Last.Animation;
		OutAlpha = 0.0f;
		return;
	}

	for (int32 I = 0; I + 1 < Order.Num(); ++I)
	{
		const FBlendSample& A = SampleData[Order[I]];
		const FBlendSample& B = SampleData[Order[I + 1]];
		if (X >= A.SampleValue.X && X <= B.SampleValue.X)
		{
			OutA = A.Animation;
			OutB = B.Animation;
			const float Span = B.SampleValue.X - A.SampleValue.X;
			OutAlpha = (Span > 1.0e-6f) ? ((X - A.SampleValue.X) / Span) : 0.0f;
			return;
		}
	}
}
