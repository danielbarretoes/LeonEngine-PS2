#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/CharacterAnimInstance.h"
#include "Animation/Skeleton.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The animation assets and anim instances (they were AnimationCore's plain structs until P14). The objects are made
// with NewObject and used before any collection, so they need no owner.

namespace
{

	// Translation matrices have the same memory as glm::translate, the layout the bone matrices keep.
	USkeleton* MakeTwoBoneSkeleton()
	{
		FReferenceSkeleton Bones;
		Bones.BoneNames = {FName("root"), FName("child")};
		Bones.ParentIndices = {INDEX_NONE, 0};
		Bones.InverseBindPose = {FMatrix::Identity, FTranslationMatrix(FVector(0.0f, -1.0f, 0.0f))};
		USkeleton* Skeleton = NewObject<USkeleton>();
		Skeleton->SetReferenceSkeleton(Bones);
		return Skeleton;
	}

	/** A clip whose frames hold the given key per bone: Frames[frame][bone]. */
	UAnimSequence* MakeClip(const TArray<TArray<FMatrix>>& Frames, float Length, float Rate, bool bInLoop = true)
	{
		TArray<FRawAnimSequenceTrack> Tracks;
		const int32 NumBones = Frames.Num() > 0 ? Frames[0].Num() : 0;
		Tracks.SetNum(NumBones);
		for (const TArray<FMatrix>& Frame : Frames)
		{
			for (int32 Bone = 0; Bone < NumBones; ++Bone)
			{
				Tracks[Bone].Keys.Add(Frame[Bone]);
			}
		}
		UAnimSequence* Clip = NewObject<UAnimSequence>();
		Clip->SequenceLength = Length;
		Clip->FrameRate = Rate;
		Clip->bLoop = bInLoop;
		Clip->SetRawAnimationData(MoveTemp(Tracks));
		return Clip;
	}

	UAnimSequence* MakeTranslatedClip(const FVector& ChildLocalTranslation)
	{
		return MakeClip({{FMatrix::Identity, FTranslationMatrix(ChildLocalTranslation)}}, 1.0f, 1.0f);
	}

	UAnimSequence* MakeTwoBoneIdentityClip()
	{
		return MakeClip({{FMatrix::Identity, FMatrix::Identity}}, 1.0f, 1.0f);
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimSequenceLoopTest, "System.Engine.Animation.Sequence.Loop",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimSequenceLoopTest::RunTest(const FString& Parameters)
{
	UAnimSequence& Clip = *MakeClip({{FMatrix::Identity}, {FTranslationMatrix(FVector(1.0f, 0.0f, 0.0f))}}, 2.0f, 1.0f);
	TestEqual("Frames", Clip.GetNumberOfFrames(), 2);

	TArray<FMatrix> Pose;
	Clip.GetBonePose(0.0f, Pose);
	TestEqual("Bones", Pose.Num(), 1);
	TestEqual("Start", Pose[0].M[3][0], 0.0f, 1.0e-4f);

	Clip.GetBonePose(2.0f, Pose); // wraps to the start
	TestEqual("Wrapped", Pose[0].M[3][0], 0.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimSequenceOneShotTest, "System.Engine.Animation.Sequence.OneShot",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimSequenceOneShotTest::RunTest(const FString& Parameters)
{
	// A one-shot clip clamps to its last frame and reports when it is finished.
	UAnimSequence& Clip = *MakeClip(
		{{FMatrix::Identity}, {FTranslationMatrix(FVector(2.0f, 0.0f, 0.0f))}}, 1.0f, 1.0f, /*bInLoop =*/false);

	TestFalse("Not finished at the start", Clip.IsFinished(0.0f));
	TestTrue("Finished at the end", Clip.IsFinished(1.0f));

	TArray<FMatrix> Pose;
	Clip.GetBonePose(5.0f, Pose);
	TestEqual("Clamped", Pose[0].M[3][0], 2.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimSequenceLerpTest, "System.Engine.Animation.Sequence.LerpMidFrame",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimSequenceLerpTest::RunTest(const FString& Parameters)
{
	// Halfway between two frames the translation is halfway too.
	UAnimSequence& Clip = *MakeClip({{FMatrix::Identity}, {FTranslationMatrix(FVector(2.0f, 0.0f, 0.0f))}}, 1.0f, 1.0f);

	TArray<FMatrix> Pose;
	Clip.GetBonePose(0.5f, Pose);
	TestEqual("Bones", Pose.Num(), 1);
	TestEqual("Translation", Pose[0].M[3][0], 1.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendSpace1DEvaluateTest, "System.Engine.Animation.BlendSpace1D.Evaluate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBlendSpace1DEvaluateTest::RunTest(const FString& Parameters)
{
	UAnimSequence* Idle = MakeTwoBoneIdentityClip();
	UAnimSequence* Run = MakeTwoBoneIdentityClip();

	UBlendSpace1D& Bs = *NewObject<UBlendSpace1D>();
	TestEqual("Default axis min", Bs.GetBlendParameter(0).Min, 0.0f, 1.0e-6f);
	TestEqual("Default axis max", Bs.GetBlendParameter(0).Max, 1.0f, 1.0e-6f);
	Bs.AddSample(Idle, 0.0f);
	Bs.AddSample(Run, 1.0f);

	const UAnimSequence* A = nullptr;
	const UAnimSequence* B = nullptr;
	float Alpha = -1.0f;

	// At idle.
	Bs.Evaluate(0.0f, A, B, Alpha);
	TestTrue("Idle: both samples idle", A == Idle && B == Idle);
	TestEqual("Idle: alpha", Alpha, 0.0f, 1.0e-5f);

	// Mid blend.
	Bs.Evaluate(0.5f, A, B, Alpha);
	TestTrue("Mid: idle to run", A == Idle && B == Run);
	TestEqual("Mid: alpha", Alpha, 0.5f, 1.0e-5f);

	// At run.
	Bs.Evaluate(1.0f, A, B, Alpha);
	TestTrue("Run: both samples run", A == Run && B == Run);
	TestEqual("Run: alpha", Alpha, 0.0f, 1.0e-5f);

	// Clamped below and above the axis.
	Bs.Evaluate(-2.0f, A, B, Alpha);
	TestTrue("Below the axis: idle", A == Idle && B == Idle);
	Bs.Evaluate(3.0f, A, B, Alpha);
	TestTrue("Above the axis: run", A == Run && B == Run);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendSpace1DEmptyTest, "System.Engine.Animation.BlendSpace1D.Empty",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBlendSpace1DEmptyTest::RunTest(const FString& Parameters)
{
	UBlendSpace1D& Bs = *NewObject<UBlendSpace1D>();
	const UAnimSequence* A = reinterpret_cast<const UAnimSequence*>(1);
	const UAnimSequence* B = reinterpret_cast<const UAnimSequence*>(1);
	float Alpha = 1.0f;
	Bs.Evaluate(0.5f, A, B, Alpha);
	TestTrue("No samples", A == nullptr && B == nullptr);
	TestEqual("Alpha", Alpha, 0.0f, 1.0e-5f);
	TestFalse("A null clip is not a sample", Bs.AddSample(nullptr, 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimInstanceNoSkeletonTest, "System.Engine.Animation.AnimInstance.NoSkeleton",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimInstanceNoSkeletonTest::RunTest(const FString& Parameters)
{
	UAnimInstance& Anim = *NewObject<UAnimInstance>();
	Anim.NativeUpdateAnimation(0.016f);
	TArray<FMatrix> Skin;
	Anim.GetSkinMatrices(Skin);
	TestEqual("No skin without a skeleton", Skin.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimInstanceSkinTest, "System.Engine.Animation.AnimInstance.BlendSpaceSkin",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimInstanceSkinTest::RunTest(const FString& Parameters)
{
	USkeleton* Skeleton = MakeTwoBoneSkeleton();
	UBlendSpace1D& Bs = *NewObject<UBlendSpace1D>();
	Bs.AddSample(MakeTranslatedClip(FVector(0.0f, 1.0f, 0.0f)), 0.0f);
	Bs.AddSample(MakeTranslatedClip(FVector(0.0f, 2.0f, 0.0f)), 1.0f);

	{
		// Idle input: the child is at its bind pose, so its skin matrix is the identity.
		UAnimInstance& Anim = *NewObject<UAnimInstance>();
		Anim.SetSkeleton(Skeleton);
		Anim.SetBlendSpace(&Bs);
		Anim.SetLocomotionBlendInterpSpeed(0.0f); // snap for unit tests
		Anim.SetBlendSpaceInput(0.0f);
		Anim.NativeUpdateAnimation(0.016f);
		TArray<FMatrix> Skin;
		Anim.GetSkinMatrices(Skin);
		TestEqual("Idle: bones", Skin.Num(), 2);
		TestEqual("Idle: child at bind", Skin[1].M[3][1], 0.0f, 1.0e-3f);
		TestEqual("Idle: alpha", Anim.GetBlendAlpha(), 0.0f, 1.0e-5f);
	}
	{
		// Mid input blends.
		UAnimInstance& Anim = *NewObject<UAnimInstance>();
		Anim.SetSkeleton(Skeleton);
		Anim.SetBlendSpace(&Bs);
		Anim.SetLocomotionBlendInterpSpeed(0.0f);
		Anim.SetBlendSpaceInput(0.5f);
		Anim.NativeUpdateAnimation(0.016f);
		TestEqual("Mid: alpha", Anim.GetBlendAlpha(), 0.5f, 1.0e-5f);
		TArray<FMatrix> Skin;
		Anim.GetSkinMatrices(Skin);
		TestEqual("Mid: bones", Skin.Num(), 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimInstanceEaseTest, "System.Engine.Animation.AnimInstance.EaseBlendInput",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimInstanceEaseTest::RunTest(const FString& Parameters)
{
	UBlendSpace1D& Bs = *NewObject<UBlendSpace1D>();
	Bs.AddSample(MakeTranslatedClip(FVector(0.0f, 1.0f, 0.0f)), 0.0f);
	Bs.AddSample(MakeTranslatedClip(FVector(0.0f, 2.0f, 0.0f)), 1.0f);

	UAnimInstance& Anim = *NewObject<UAnimInstance>();
	Anim.SetSkeleton(MakeTwoBoneSkeleton());
	Anim.SetBlendSpace(&Bs);
	Anim.SetLocomotionBlendInterpSpeed(8.0f);
	Anim.SetBlendSpaceInput(1.0f);
	Anim.NativeUpdateAnimation(0.016f);
	TestTrue("Moving toward the target", Anim.GetBlendSpaceInput() > 0.0f && Anim.GetBlendSpaceInput() < 1.0f);
	TestEqual("Target", Anim.GetBlendSpaceInputTarget(), 1.0f, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterAnimJumpTest,
	"System.Engine.Animation.CharacterAnimInstance.JumpStateMachine",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterAnimJumpTest::RunTest(const FString& Parameters)
{
	UAnimSequence* Jump = MakeTranslatedClip(FVector(0.0f, 3.0f, 0.0f));
	UAnimSequence* Fall = MakeTranslatedClip(FVector(0.0f, 4.0f, 0.0f));
	UAnimSequence* Land = MakeTranslatedClip(FVector(0.0f, 1.5f, 0.0f));
	Jump->bLoop = false;
	Jump->SequenceLength = 0.2f;
	Fall->bLoop = true;
	Land->bLoop = false;
	Land->SequenceLength = 0.2f;

	UBlendSpace1D& Bs = *NewObject<UBlendSpace1D>();
	Bs.AddSample(MakeTranslatedClip(FVector(0.0f, 1.0f, 0.0f)), 0.0f);
	Bs.AddSample(MakeTranslatedClip(FVector(0.0f, 2.0f, 0.0f)), 1.0f);

	UCharacterAnimInstance& Anim = *NewObject<UCharacterAnimInstance>();
	Anim.SetSkeleton(MakeTwoBoneSkeleton());
	Anim.SetBlendSpace(&Bs);
	Anim.SetJumpClips({Jump, Fall, Land});
	Anim.SetCrossfadeDuration(0.1f);
	Anim.SetJumpPlayRates(1.0f, 1.0f, 1.0f);
	Anim.SetLocomotionBlendInterpSpeed(0.0f);
	Anim.SetBlendSpaceInput(0.0f);

	TestTrue("Starts in locomotion", Anim.GetJumpState() == EAnimJumpState::Locomotion);

	Anim.NotifyJumped();
	Anim.SetMovementState(true, 5.0f, false);
	Anim.NativeUpdateAnimation(0.016f);
	TestTrue("Jump start", Anim.GetJumpState() == EAnimJumpState::JumpStart);
	TestTrue("Crossfading", Anim.GetCrossfadeAlpha() < 1.0f);

	Anim.SetMovementState(true, -1.0f, false);
	Anim.NativeUpdateAnimation(0.016f);
	TestTrue("Falling", Anim.GetJumpState() == EAnimJumpState::FallLoop);

	Anim.SetMovementState(false, 0.0f, true);
	Anim.NativeUpdateAnimation(0.016f);
	TestTrue("Landing", Anim.GetJumpState() == EAnimJumpState::Land);

	for (int32 I = 0; I < 20; ++I)
	{
		Anim.SetMovementState(false, 0.0f, false);
		Anim.NativeUpdateAnimation(0.05f);
	}
	TestTrue("Back to locomotion", Anim.GetJumpState() == EAnimJumpState::Locomotion);

	TArray<FMatrix> Skin;
	Anim.GetSkinMatrices(Skin);
	TestEqual("Skin bones", Skin.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterAnimPlayRateTest,
	"System.Engine.Animation.CharacterAnimInstance.JumpPlayRate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterAnimPlayRateTest::RunTest(const FString& Parameters)
{
	// A faster jump play rate finishes the one-shot sooner.
	UAnimSequence* Jump = MakeTranslatedClip(FVector(0.0f, 3.0f, 0.0f));
	UAnimSequence* Fall = MakeTranslatedClip(FVector(0.0f, 4.0f, 0.0f));
	Jump->bLoop = false;
	Jump->SequenceLength = 1.0f;
	Fall->bLoop = true;

	UBlendSpace1D& Bs = *NewObject<UBlendSpace1D>();
	Bs.AddSample(MakeTranslatedClip(FVector(0.0f, 1.0f, 0.0f)), 0.0f);

	UCharacterAnimInstance& Anim = *NewObject<UCharacterAnimInstance>();
	Anim.SetSkeleton(MakeTwoBoneSkeleton());
	Anim.SetBlendSpace(&Bs);
	Anim.SetJumpClips({Jump, Fall, nullptr});
	Anim.SetCrossfadeDuration(0.0f);
	Anim.SetJumpPlayRates(4.0f, 1.0f, 1.0f);
	Anim.SetLocomotionBlendInterpSpeed(0.0f);

	Anim.NotifyJumped();
	Anim.SetMovementState(true, 5.0f, false);
	Anim.NativeUpdateAnimation(0.0f);
	TestTrue("Jump start", Anim.GetJumpState() == EAnimJumpState::JumpStart);

	Anim.SetMovementState(true, 5.0f, false);
	Anim.NativeUpdateAnimation(0.3f);
	TestTrue("Falling after 0.3 s at 4x", Anim.GetJumpState() == EAnimJumpState::FallLoop);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
