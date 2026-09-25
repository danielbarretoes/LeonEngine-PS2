#include "Animation/AnimInstance.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"

UAnimInstance::UAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UAnimInstance::GetNumBones() const
{
	return Skeleton != nullptr ? Skeleton->GetReferenceSkeleton().GetNum() : 0;
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
	const int32 LocalBoneCount = GetNumBones();
	const bool bHasA = SampleA != nullptr && SampleA->GetNumberOfFrames() > 0;
	const bool bHasB = SampleB != nullptr && SampleB->GetNumberOfFrames() > 0;
	if (!bHasA && !bHasB)
	{
		OutBoneWorld.Init(FMatrix::Identity, LocalBoneCount);
		return;
	}

	TArray<FMatrix> WorldA;
	TArray<FMatrix> WorldB;
	if (bHasA)
	{
		SampleA->GetBonePose(TimeA, WorldA);
	}
	if (bHasB)
	{
		SampleB->GetBonePose(TimeB, WorldB);
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
	const int32 LocalBoneCount = GetNumBones();
	if (LocalBoneCount <= 0)
	{
		return;
	}
	OutSkin.Init(FMatrix::Identity, LocalBoneCount);
	if (BoneWorld.Num() != LocalBoneCount)
	{
		return;
	}
	const TArray<FMatrix>& InverseBindPose = Skeleton->GetReferenceSkeleton().InverseBindPose;
	for (int32 I = 0; I < LocalBoneCount && I < InverseBindPose.Num(); ++I)
	{
		// Inverse bind first, then the bone's world matrix (row-vector product order; glm: BoneWorld * InverseBind).
		OutSkin[I] = InverseBindPose[I] * BoneWorld[I];
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
