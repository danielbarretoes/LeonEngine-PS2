#include "SkeletalAnimation.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <unordered_map> // IWYU pragma: keep — used below; include-cleaner false positive

int USkeleton::FindBoneIndex(const std::string& InName) const
{
	for (int I = 0; I < BoneCount(); ++I)
	{
		if (BoneNames[static_cast<std::size_t>(I)] == InName)
		{
			return I;
		}
	}
	return -1;
}

bool UAnimSequence::IsFinished(float TimeSeconds) const
{
	if (bLooping || DurationSeconds <= 1.0e-4f)
	{
		return false;
	}
	return TimeSeconds >= (DurationSeconds - 1.0e-4f);
}

void UAnimSequence::SampleLocalPose(float TimeSeconds, std::vector<glm::mat4>& OutBoneWorld) const
{
	const int LocalBoneCount = FrameCount() > 0 ? static_cast<int>(LocalPoseFrames[0].size()) : 0;
	OutBoneWorld.assign(static_cast<std::size_t>(LocalBoneCount), glm::mat4(1.0f));
	if (LocalBoneCount <= 0 || FrameCount() <= 0)
	{
		return;
	}

	float T = TimeSeconds;
	if (DurationSeconds > 1.0e-4f)
	{
		if (bLooping)
		{
			T = std::fmod(T, DurationSeconds);
			if (T < 0.0f)
			{
				T += DurationSeconds;
			}
		}
		else
		{
			T = std::clamp(T, 0.0f, DurationSeconds);
		}
	}
	const float FrameF = T * FramesPerSecond;
	int F0 = 0;
	int F1 = 0;
	float Alpha = 0.0f;
	if (bLooping)
	{
		F0 = static_cast<int>(FrameF) % FrameCount();
		F1 = (F0 + 1) % FrameCount();
		Alpha = FrameF - std::floor(FrameF);
	}
	else
	{
		const float MaxFrame = static_cast<float>(FrameCount() - 1);
		const float Clamped = std::min(FrameF, MaxFrame);
		F0 = static_cast<int>(Clamped);
		F1 = std::min(F0 + 1, FrameCount() - 1);
		Alpha = Clamped - std::floor(Clamped);
	}

	const auto& A = LocalPoseFrames[static_cast<std::size_t>(F0)];
	const auto& B = LocalPoseFrames[static_cast<std::size_t>(F1)];
	for (int I = 0; I < LocalBoneCount; ++I)
	{
		// Matrix lerp is approximate but fine for a micro blend-space / crossfade.
		OutBoneWorld[static_cast<std::size_t>(I)] =
			A[static_cast<std::size_t>(I)] * (1.0f - Alpha) + B[static_cast<std::size_t>(I)] * Alpha;
	}
}

void UBlendSpace1D::Evaluate(
	float AxisValue, const UAnimSequence*& OutA, const UAnimSequence*& OutB, float& OutAlpha) const
{
	OutA = nullptr;
	OutB = nullptr;
	OutAlpha = 0.0f;
	if (Samples.empty())
	{
		return;
	}

	// Sort indices by sample position (stable for typical Idle@0 / Run@1 authoring).
	std::vector<std::size_t> Order(Samples.size());
	for (std::size_t I = 0; I < Samples.size(); ++I)
	{
		Order[I] = I;
	}
	std::sort(Order.begin(), Order.end(),
		[this](std::size_t A, std::size_t B) { return Samples[A].Position < Samples[B].Position; });

	const float X = std::clamp(AxisValue, AxisMin, AxisMax);
	const FBlendSample& First = Samples[Order.front()];
	const FBlendSample& Last = Samples[Order.back()];
	if (X <= First.Position || Order.size() == 1)
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

	for (std::size_t I = 0; I + 1 < Order.size(); ++I)
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
		const float T = 1.0f - std::exp(-LocomotionBlendInterpSpeed * DeltaTime);
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

void UAnimInstance::SampleLocomotionBoneWorld(std::vector<glm::mat4>& OutBoneWorld) const
{
	OutBoneWorld.clear();
	if (Skeleton == nullptr)
	{
		return;
	}
	const int LocalBoneCount = Skeleton->BoneCount();
	const bool bHasA = SampleA != nullptr && SampleA->FrameCount() > 0;
	const bool bHasB = SampleB != nullptr && SampleB->FrameCount() > 0;
	if (!bHasA && !bHasB)
	{
		OutBoneWorld.assign(static_cast<std::size_t>(LocalBoneCount), glm::mat4(1.0f));
		return;
	}

	std::vector<glm::mat4> WorldA;
	std::vector<glm::mat4> WorldB;
	if (bHasA)
	{
		SampleA->SampleLocalPose(TimeA, WorldA);
	}
	if (bHasB)
	{
		SampleB->SampleLocalPose(TimeB, WorldB);
	}

	OutBoneWorld.resize(static_cast<std::size_t>(LocalBoneCount), glm::mat4(1.0f));
	for (int I = 0; I < LocalBoneCount; ++I)
	{
		const glm::mat4 A = (WorldA.size() == static_cast<std::size_t>(LocalBoneCount))
			? WorldA[static_cast<std::size_t>(I)]
			: (WorldB.size() == static_cast<std::size_t>(LocalBoneCount) ? WorldB[static_cast<std::size_t>(I)]
																		 : glm::mat4(1.0f));
		const glm::mat4 B =
			(WorldB.size() == static_cast<std::size_t>(LocalBoneCount)) ? WorldB[static_cast<std::size_t>(I)] : A;
		OutBoneWorld[static_cast<std::size_t>(I)] = A * (1.0f - BlendAlpha) + B * BlendAlpha;
	}
}

void UAnimInstance::SkinFromBoneWorld(const std::vector<glm::mat4>& BoneWorld, std::vector<glm::mat4>& OutSkin) const
{
	OutSkin.clear();
	if (Skeleton == nullptr || Skeleton->BoneCount() <= 0)
	{
		return;
	}
	const int LocalBoneCount = Skeleton->BoneCount();
	if (BoneWorld.size() != static_cast<std::size_t>(LocalBoneCount))
	{
		OutSkin.assign(static_cast<std::size_t>(LocalBoneCount), glm::mat4(1.0f));
		return;
	}
	OutSkin.resize(static_cast<std::size_t>(LocalBoneCount), glm::mat4(1.0f));
	for (int I = 0; I < LocalBoneCount; ++I)
	{
		OutSkin[static_cast<std::size_t>(I)] =
			BoneWorld[static_cast<std::size_t>(I)] * Skeleton->InverseBindPose[static_cast<std::size_t>(I)];
	}
}

void UAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	UpdateLocomotion(DeltaTime);
}

void UAnimInstance::GetBoneWorldMatrices(std::vector<glm::mat4>& OutBoneWorld) const
{
	SampleLocomotionBoneWorld(OutBoneWorld);
}

void UAnimInstance::GetSkinMatrices(std::vector<glm::mat4>& OutSkin) const
{
	std::vector<glm::mat4> World;
	GetBoneWorldMatrices(World);
	SkinFromBoneWorld(World, OutSkin);
}
