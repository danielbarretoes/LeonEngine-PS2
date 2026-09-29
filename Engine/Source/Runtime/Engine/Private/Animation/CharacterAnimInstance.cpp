#include "Animation/CharacterAnimInstance.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "Misc/MemStack.h"

UCharacterAnimInstance::UCharacterAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCharacterAnimInstance::SetJumpPlayRates(float JumpStart, float FallLoop, float Land)
{
	JumpStartPlayRate = FMath::Max(JumpStart, 0.01f);
	FallLoopPlayRate = FMath::Max(FallLoop, 0.01f);
	LandPlayRate = FMath::Max(Land, 0.01f);
}

void UCharacterAnimInstance::NotifyJumped()
{
	bJumpRequested = true;
}

void UCharacterAnimInstance::SetMovementState(bool bInFalling, float InVelocityZ, bool bInJustLanded)
{
	bFalling = bInFalling;
	VelocityZ = InVelocityZ;
	bJustLanded = bInJustLanded;
}

float UCharacterAnimInstance::PlayRateForState(EAnimJumpState State) const
{
	switch (State)
	{
		case EAnimJumpState::JumpStart:
			return JumpStartPlayRate;
		case EAnimJumpState::FallLoop:
			return FallLoopPlayRate;
		case EAnimJumpState::Land:
			return LandPlayRate;
		case EAnimJumpState::Locomotion:
		default:
			return 1.0f;
	}
}

void UCharacterAnimInstance::EnterState(EAnimJumpState Next)
{
	if (Next == JumpState)
	{
		return;
	}

	const EAnimJumpState From = JumpState;
	PreviousState = From;
	Previous = Active;

	JumpState = Next;
	Active = {};
	switch (Next)
	{
		case EAnimJumpState::JumpStart:
			Active.Sequence = JumpClips.JumpStart;
			break;
		case EAnimJumpState::FallLoop:
			Active.Sequence = JumpClips.FallLoop;
			break;
		case EAnimJumpState::Land:
			Active.Sequence = JumpClips.Land;
			break;
		case EAnimJumpState::Locomotion:
		default:
			Active.Sequence = nullptr;
			break;
	}
	Active.Time = 0.0f;

	float Fade = CrossfadeDuration;
	if (From == EAnimJumpState::Land && Next == EAnimJumpState::Locomotion)
	{
		Fade = FMath::Max(CrossfadeDuration, LandToLocomotionCrossfade);
	}
	CrossfadeElapsed = 0.0f;
	CrossfadeAlpha = (Fade <= 1.0e-6f) ? 1.0f : 0.0f;
	ActiveCrossfadeDuration = Fade;
}

void UCharacterAnimInstance::UpdateJumpStateMachine()
{
	const bool bHasJumpStart = JumpClips.JumpStart != nullptr && JumpClips.JumpStart->GetNumberOfFrames() > 0;
	const bool bHasFallLoop = JumpClips.FallLoop != nullptr && JumpClips.FallLoop->GetNumberOfFrames() > 0;
	const bool bHasLand = JumpClips.Land != nullptr && JumpClips.Land->GetNumberOfFrames() > 0;

	switch (JumpState)
	{
		case EAnimJumpState::Locomotion:
			if (bJumpRequested)
			{
				bJumpRequested = false;
				if (bHasJumpStart)
				{
					EnterState(EAnimJumpState::JumpStart);
				}
				else if (bHasFallLoop)
				{
					EnterState(EAnimJumpState::FallLoop);
				}
			}
			else if (bFalling)
			{
				if (VelocityZ > 0.0f && bHasJumpStart)
				{
					EnterState(EAnimJumpState::JumpStart);
				}
				else if (bHasFallLoop)
				{
					EnterState(EAnimJumpState::FallLoop);
				}
			}
			break;

		case EAnimJumpState::JumpStart:
			bJumpRequested = false;
			if (bJustLanded || !bFalling)
			{
				if (bHasLand)
				{
					EnterState(EAnimJumpState::Land);
				}
				else
				{
					EnterState(EAnimJumpState::Locomotion);
				}
			}
			else if (VelocityZ <= 0.0f || (Active.Sequence != nullptr && Active.Sequence->IsFinished(Active.Time)))
			{
				if (bHasFallLoop)
				{
					EnterState(EAnimJumpState::FallLoop);
				}
			}
			break;

		case EAnimJumpState::FallLoop:
			bJumpRequested = false;
			if (bJustLanded || !bFalling)
			{
				if (bHasLand)
				{
					EnterState(EAnimJumpState::Land);
				}
				else
				{
					EnterState(EAnimJumpState::Locomotion);
				}
			}
			break;

		case EAnimJumpState::Land:
			if (bJumpRequested && bFalling)
			{
				bJumpRequested = false;
				if (bHasJumpStart)
				{
					EnterState(EAnimJumpState::JumpStart);
				}
				else if (bHasFallLoop)
				{
					EnterState(EAnimJumpState::FallLoop);
				}
			}
			else if (bFalling && !bJustLanded)
			{
				bJumpRequested = false;
				if (bHasFallLoop)
				{
					EnterState(EAnimJumpState::FallLoop);
				}
			}
			else if (Active.Sequence == nullptr || Active.Sequence->IsFinished(Active.Time) || !bHasLand)
			{
				bJumpRequested = false;
				EnterState(EAnimJumpState::Locomotion);
			}
			else
			{
				bJumpRequested = false;
			}
			break;
	}

	bJustLanded = false;
}

void UCharacterAnimInstance::SamplePlayerPose(const FAnimPosePlayer& Player, TArrayView<FTransform> OutPose) const
{
	SampleSequencePose(Player.Sequence, Player.Time, OutPose);
}

void UCharacterAnimInstance::UpdateCrouch(float DeltaTime)
{
	// The standing space is whatever the owner set last (SetBlendSpace): anything but what this update set.
	const UBlendSpaceBase* Current = GetBlendSpace();
	if (!bAppliedBlendSpace || Current != AppliedBlendSpace)
	{
		StandBlendSpace = Current;
	}
	bCrouchActive = bCrouched && CrouchBlendSpace != nullptr;
	const float Target = bCrouchActive ? 1.0f : 0.0f;
	const float Step = CrossfadeDuration > 1.0e-6f ? DeltaTime / CrossfadeDuration : 1.0f;
	CrouchAlpha =
		Target > CrouchAlpha ? FMath::Min(CrouchAlpha + Step, Target) : FMath::Max(CrouchAlpha - Step, Target);
	AppliedBlendSpace = bCrouchActive ? CrouchBlendSpace : StandBlendSpace;
	bAppliedBlendSpace = true;
	SetBlendSpace(AppliedBlendSpace);
}

void UCharacterAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	UpdateCrouch(DeltaTime);
	// The playing space advances the time and fires the notifies (the crouched walk is silent, as in CS); the one
	// fading out follows the same input and time.
	UpdateLocomotion(DeltaTime);
	FadingSamples.Reset();
	const UBlendSpaceBase* Fading = bCrouchActive ? StandBlendSpace : CrouchBlendSpace;
	const float FadingWeight = bCrouchActive ? 1.0f - CrouchAlpha : CrouchAlpha;
	if (Fading != nullptr && FadingWeight > 1.0e-3f)
	{
		Fading->GetSamplesFromBlendInput(GetBlendSpaceInput(), FadingSamples);
	}

	// The active state's clip fires its notifies; the one fading out does not.
	if (JumpState != EAnimJumpState::Locomotion)
	{
		AdvanceSequencePlayer(Active.Sequence, Active.Time, DeltaTime * PlayRateForState(JumpState), true);
	}
	if (CrossfadeAlpha < 1.0f && PreviousState != EAnimJumpState::Locomotion)
	{
		AdvanceSequencePlayer(Previous.Sequence, Previous.Time, DeltaTime * PlayRateForState(PreviousState), false);
	}

	UpdateJumpStateMachine();

	if (CrossfadeAlpha < 1.0f)
	{
		const float FadeDur = ActiveCrossfadeDuration > 1.0e-6f ? ActiveCrossfadeDuration : CrossfadeDuration;
		if (FadeDur <= 1.0e-6f)
		{
			CrossfadeAlpha = 1.0f;
			CrossfadeElapsed = FadeDur;
		}
		else
		{
			CrossfadeElapsed += DeltaTime;
			const float T = FMath::Clamp(CrossfadeElapsed / FadeDur, 0.0f, 1.0f);
			CrossfadeAlpha = T * T * (3.0f - (2.0f * T));
		}
	}
}

void UCharacterAnimInstance::SampleCrouchedLocomotionPose(TArrayView<FTransform> OutPose) const
{
	SampleLocomotionPose(OutPose);
	if (FadingSamples.Num() == 0)
	{
		return;
	}
	const UBlendSpaceBase* Fading = bCrouchActive ? StandBlendSpace : CrouchBlendSpace;
	const float FadingWeight = bCrouchActive ? 1.0f - CrouchAlpha : CrouchAlpha;
	FMemMark Mark(FMemStack::Get());
	TArray<FTransform, TMemStackAllocator<>> FadingPose;
	FadingPose.SetNum(OutPose.Num());
	SampleBlendSpacePose(Fading, FadingSamples, GetLocomotionNormalizedTime(), FadingPose);
	FAnimationRuntime::BlendTwoPosesTogether(OutPose, FadingPose, FadingWeight, OutPose);
}

void UCharacterAnimInstance::EvaluateBasePose(TArrayView<FTransform> OutPose) const
{
	if (JumpState == EAnimJumpState::Locomotion)
	{
		SampleCrouchedLocomotionPose(OutPose);
	}
	else
	{
		SamplePlayerPose(Active, OutPose);
	}
	if (CrossfadeAlpha >= 0.999f)
	{
		return;
	}
	// The previous state's pose on the frame's stack.
	FMemMark Mark(FMemStack::Get());
	TArray<FTransform, TMemStackAllocator<>> PreviousPose;
	PreviousPose.SetNum(OutPose.Num());
	if (PreviousState == EAnimJumpState::Locomotion)
	{
		SampleCrouchedLocomotionPose(PreviousPose);
	}
	else
	{
		SamplePlayerPose(Previous, PreviousPose);
	}
	FAnimationRuntime::BlendTwoPosesTogether(PreviousPose, OutPose, CrossfadeAlpha, OutPose);
}
