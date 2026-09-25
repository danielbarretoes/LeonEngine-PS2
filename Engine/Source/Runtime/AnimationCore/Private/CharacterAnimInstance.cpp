#include "SkeletalAnimation.h"

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

void UCharacterAnimInstance::AdvancePlayer(FPosePlayer& Player, float DeltaTime, float PlayRate) const
{
	if (Player.Sequence == nullptr)
	{
		return;
	}
	Player.Time += DeltaTime * PlayRate;
	if (!Player.Sequence->bLooping && Player.Sequence->DurationSeconds > 1.0e-4f)
	{
		Player.Time = FMath::Min(Player.Time, Player.Sequence->DurationSeconds);
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
	const bool bHasJumpStart = JumpClips.JumpStart != nullptr && JumpClips.JumpStart->FrameCount() > 0;
	const bool bHasFallLoop = JumpClips.FallLoop != nullptr && JumpClips.FallLoop->FrameCount() > 0;
	const bool bHasLand = JumpClips.Land != nullptr && JumpClips.Land->FrameCount() > 0;

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

void UCharacterAnimInstance::SamplePlayerBoneWorld(const FPosePlayer& Player, TArray<FMatrix>& OutBoneWorld) const
{
	OutBoneWorld.Reset();
	const USkeleton* LocalSkeleton = GetSkeleton();
	if (LocalSkeleton == nullptr)
	{
		return;
	}
	if (Player.Sequence == nullptr || Player.Sequence->FrameCount() <= 0)
	{
		OutBoneWorld.Init(FMatrix::Identity, LocalSkeleton->BoneCount());
		return;
	}
	Player.Sequence->SampleLocalPose(Player.Time, OutBoneWorld);
}

void UCharacterAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
	UpdateLocomotion(DeltaTime);

	if (JumpState != EAnimJumpState::Locomotion)
	{
		AdvancePlayer(Active, DeltaTime, PlayRateForState(JumpState));
	}
	if (CrossfadeAlpha < 1.0f && PreviousState != EAnimJumpState::Locomotion)
	{
		AdvancePlayer(Previous, DeltaTime, PlayRateForState(PreviousState));
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

void UCharacterAnimInstance::GetBoneWorldMatrices(TArray<FMatrix>& OutBoneWorld) const
{
	const USkeleton* LocalSkeleton = GetSkeleton();
	if (LocalSkeleton == nullptr || LocalSkeleton->BoneCount() <= 0)
	{
		OutBoneWorld.Reset();
		return;
	}

	const int32 BoneCount = LocalSkeleton->BoneCount();
	TArray<FMatrix> WorldCurrent;
	if (JumpState == EAnimJumpState::Locomotion)
	{
		SampleLocomotionBoneWorld(WorldCurrent);
	}
	else
	{
		SamplePlayerBoneWorld(Active, WorldCurrent);
	}

	OutBoneWorld = WorldCurrent;
	if (CrossfadeAlpha < 0.999f)
	{
		TArray<FMatrix> WorldPrev;
		if (PreviousState == EAnimJumpState::Locomotion)
		{
			SampleLocomotionBoneWorld(WorldPrev);
		}
		else
		{
			SamplePlayerBoneWorld(Previous, WorldPrev);
		}
		if (WorldPrev.Num() == BoneCount && WorldCurrent.Num() == BoneCount)
		{
			OutBoneWorld.SetNum(BoneCount);
			for (int32 I = 0; I < BoneCount; ++I)
			{
				OutBoneWorld[I] = WorldPrev[I] * (1.0f - CrossfadeAlpha) + WorldCurrent[I] * CrossfadeAlpha;
			}
		}
	}
}

void UCharacterAnimInstance::GetSkinMatrices(TArray<FMatrix>& OutSkin) const
{
	TArray<FMatrix> WorldBlended;
	GetBoneWorldMatrices(WorldBlended);
	SkinFromBoneWorld(WorldBlended, OutSkin);
}
