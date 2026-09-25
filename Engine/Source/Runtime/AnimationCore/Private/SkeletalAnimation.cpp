#include "SkeletalAnimation.h"

int32 USkeleton::FindBoneIndex(FName InName) const
{
	return BoneNames.IndexOfByKey(InName);
}

bool UAnimSequence::IsFinished(float TimeSeconds) const
{
	if (bLooping || DurationSeconds <= 1.0e-4f)
	{
		return false;
	}
	return TimeSeconds >= (DurationSeconds - 1.0e-4f);
}

void UAnimSequence::SampleLocalPose(float TimeSeconds, TArray<FMatrix>& OutBoneWorld) const
{
	const int32 LocalBoneCount = FrameCount() > 0 ? LocalPoseFrames[0].Num() : 0;
	OutBoneWorld.Init(FMatrix::Identity, LocalBoneCount);
	if (LocalBoneCount <= 0 || FrameCount() <= 0)
	{
		return;
	}

	float T = TimeSeconds;
	if (DurationSeconds > 1.0e-4f)
	{
		if (bLooping)
		{
			T = FMath::Fmod(T, DurationSeconds);
			if (T < 0.0f)
			{
				T += DurationSeconds;
			}
		}
		else
		{
			T = FMath::Clamp(T, 0.0f, DurationSeconds);
		}
	}
	const float FrameF = T * FramesPerSecond;
	int32 F0 = 0;
	int32 F1 = 0;
	float Alpha = 0.0f;
	if (bLooping)
	{
		F0 = static_cast<int32>(FrameF) % FrameCount();
		F1 = (F0 + 1) % FrameCount();
		Alpha = FrameF - FMath::FloorToFloat(FrameF);
	}
	else
	{
		const float MaxFrame = static_cast<float>(FrameCount() - 1);
		const float Clamped = FMath::Min(FrameF, MaxFrame);
		F0 = static_cast<int32>(Clamped);
		F1 = FMath::Min(F0 + 1, FrameCount() - 1);
		Alpha = Clamped - FMath::FloorToFloat(Clamped);
	}

	const TArray<FMatrix>& A = LocalPoseFrames[F0];
	const TArray<FMatrix>& B = LocalPoseFrames[F1];
	for (int32 I = 0; I < LocalBoneCount; ++I)
	{
		// Matrix lerp is approximate but fine for a micro blend-space / crossfade.
		OutBoneWorld[I] = A[I] * (1.0f - Alpha) + B[I] * Alpha;
	}
}

void UBlendSpace1D::Evaluate(
	float AxisValue, const UAnimSequence*& OutA, const UAnimSequence*& OutB, float& OutAlpha) const
{
	OutA = nullptr;
	OutB = nullptr;
	OutAlpha = 0.0f;
	if (Samples.Num() == 0)
	{
		return;
	}

	// Sample indices sorted by position (stable, so equal positions keep their authoring order).
	TArray<int32> Order;
	Order.SetNum(Samples.Num());
	for (int32 I = 0; I < Samples.Num(); ++I)
	{
		Order[I] = I;
	}
	StableSort(
		Order.GetData(), Order.Num(), [this](int32 A, int32 B) { return Samples[A].Position < Samples[B].Position; });

	const float X = FMath::Clamp(AxisValue, AxisMin, AxisMax);
	const FBlendSample& First = Samples[Order[0]];
	const FBlendSample& Last = Samples[Order.Last()];
	if (X <= First.Position || Order.Num() == 1)
	{
		OutA = First.Sequence;
		OutB = First.Sequence;
		OutAlpha = 0.0f;
		return;
	}
	if (X >= Last.Position)
	{
		OutA = Last.Sequence;
		OutB = Last.Sequence;
		OutAlpha = 0.0f;
		return;
	}

	for (int32 I = 0; I + 1 < Order.Num(); ++I)
	{
		const FBlendSample& A = Samples[Order[I]];
		const FBlendSample& B = Samples[Order[I + 1]];
		if (X >= A.Position && X <= B.Position)
		{
			OutA = A.Sequence;
			OutB = B.Sequence;
			const float Span = B.Position - A.Position;
			OutAlpha = (Span > 1.0e-6f) ? ((X - A.Position) / Span) : 0.0f;
			return;
		}
	}
}

void UAnimInstance::SetBlendSpaceInput(float AxisValue)
{
	BlendInputTarget = AxisValue;
}

void UAnimInstance::UpdateLocomotion(float DeltaTime)
{
	if (LocomotionBlendInterpSpeed <= 1.0e-6f)
	{
		BlendInput = BlendInputTarget;
	}
	else
	{
		const float T = 1.0f - FMath::Exp(-LocomotionBlendInterpSpeed * DeltaTime);
		BlendInput += (BlendInputTarget - BlendInput) * T;
	}

	SampleA = nullptr;
	SampleB = nullptr;
	BlendAlpha = 0.0f;
	if (BlendSpace != nullptr)
	{
		BlendSpace->Evaluate(BlendInput, SampleA, SampleB, BlendAlpha);
	}
	if (SampleA != nullptr)
	{
		TimeA += DeltaTime;
	}
	if (SampleB != nullptr && SampleB != SampleA)
	{
		TimeB += DeltaTime;
	}
	else if (SampleB == SampleA)
	{
		TimeB = TimeA;
	}
}

void UAnimInstance::SampleLocomotionBoneWorld(TArray<FMatrix>& OutBoneWorld) const
{
	OutBoneWorld.Reset();
	if (Skeleton == nullptr)
	{
		return;
	}
	const int32 LocalBoneCount = Skeleton->BoneCount();
	const bool bHasA = SampleA != nullptr && SampleA->FrameCount() > 0;
	const bool bHasB = SampleB != nullptr && SampleB->FrameCount() > 0;
	if (!bHasA && !bHasB)
	{
		OutBoneWorld.Init(FMatrix::Identity, LocalBoneCount);
		return;
	}

	TArray<FMatrix> WorldA;
	TArray<FMatrix> WorldB;
	if (bHasA)
	{
		SampleA->SampleLocalPose(TimeA, WorldA);
	}
	if (bHasB)
	{
		SampleB->SampleLocalPose(TimeB, WorldB);
	}

	OutBoneWorld.Init(FMatrix::Identity, LocalBoneCount);
	for (int32 I = 0; I < LocalBoneCount; ++I)
	{
		const FMatrix A = (WorldA.Num() == LocalBoneCount)
			? WorldA[I]
			: (WorldB.Num() == LocalBoneCount ? WorldB[I] : FMatrix::Identity);
		const FMatrix B = (WorldB.Num() == LocalBoneCount) ? WorldB[I] : A;
		OutBoneWorld[I] = A * (1.0f - BlendAlpha) + B * BlendAlpha;
	}
}

void UAnimInstance::SkinFromBoneWorld(const TArray<FMatrix>& BoneWorld, TArray<FMatrix>& OutSkin) const
{
	OutSkin.Reset();
	if (Skeleton == nullptr || Skeleton->BoneCount() <= 0)
	{
		return;
	}
	const int32 LocalBoneCount = Skeleton->BoneCount();
	if (BoneWorld.Num() != LocalBoneCount)
	{
		OutSkin.Init(FMatrix::Identity, LocalBoneCount);
		return;
	}
	OutSkin.Init(FMatrix::Identity, LocalBoneCount);
	for (int32 I = 0; I < LocalBoneCount; ++I)
	{
		// Inverse bind first, then the bone's world matrix (row-vector product order; glm: BoneWorld * InverseBind).
		OutSkin[I] = Skeleton->InverseBindPose[I] * BoneWorld[I];
	}
}

void UAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	UpdateLocomotion(DeltaTime);
}

void UAnimInstance::GetBoneWorldMatrices(TArray<FMatrix>& OutBoneWorld) const
{
	SampleLocomotionBoneWorld(OutBoneWorld);
}

void UAnimInstance::GetSkinMatrices(TArray<FMatrix>& OutSkin) const
{
	TArray<FMatrix> World;
	GetBoneWorldMatrices(World);
	SkinFromBoneWorld(World, OutSkin);
}
