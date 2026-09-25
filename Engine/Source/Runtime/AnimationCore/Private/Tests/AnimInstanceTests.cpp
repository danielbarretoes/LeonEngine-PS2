#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "SkeletalAnimation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	// Translation matrices have the same memory as glm::translate, the layout the bone matrices keep.
	USkeleton MakeTwoBoneSkeleton()
	{
		USkeleton Sk;
		Sk.BoneNames = {FName("root"), FName("child")};
		Sk.ParentIndices = {INDEX_NONE, 0};
		Sk.InverseBindPose = {FMatrix::Identity, FTranslationMatrix(FVector(0.0f, -1.0f, 0.0f))};
		return Sk;
	}

	UAnimSequence MakeTranslatedClip(const TCHAR* Name, const FVector& ChildLocalTranslation)
	{
		UAnimSequence Clip;
		Clip.Name = FName(Name);
		Clip.DurationSeconds = 1.0f;
		Clip.FramesPerSecond = 1.0f;
		Clip.LocalPoseFrames.SetNum(1);
		Clip.LocalPoseFrames[0] = {FMatrix::Identity, FTranslationMatrix(ChildLocalTranslation)};
		return Clip;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimSequenceLoopTest, "System.AnimationCore.Sequence.Loop",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimSequenceLoopTest::RunTest(const FString& Parameters)
{
	UAnimSequence Clip;
	Clip.DurationSeconds = 2.0f;
	Clip.FramesPerSecond = 1.0f;
	Clip.LocalPoseFrames.SetNum(2);
	Clip.LocalPoseFrames[0] = {FMatrix::Identity};
	Clip.LocalPoseFrames[1] = {FTranslationMatrix(FVector(1.0f, 0.0f, 0.0f))};

	TArray<FMatrix> Pose;
	Clip.SampleLocalPose(0.0f, Pose);
	TestEqual("Bones", Pose.Num(), 1);
	TestEqual("Start", Pose[0].M[3][0], 0.0f, 1.0e-4f);

	Clip.SampleLocalPose(2.0f, Pose); // wraps to the start
	TestEqual("Wrapped", Pose[0].M[3][0], 0.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimSequenceOneShotTest, "System.AnimationCore.Sequence.OneShot",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimSequenceOneShotTest::RunTest(const FString& Parameters)
{
	// A one-shot clip clamps to its last frame and reports when it is finished.
	UAnimSequence Clip;
	Clip.DurationSeconds = 1.0f;
	Clip.FramesPerSecond = 1.0f;
	Clip.bLooping = false;
	Clip.LocalPoseFrames.SetNum(2);
	Clip.LocalPoseFrames[0] = {FMatrix::Identity};
	Clip.LocalPoseFrames[1] = {FTranslationMatrix(FVector(2.0f, 0.0f, 0.0f))};

	TestFalse("Not finished at the start", Clip.IsFinished(0.0f));
	TestTrue("Finished at the end", Clip.IsFinished(1.0f));

	TArray<FMatrix> Pose;
	Clip.SampleLocalPose(5.0f, Pose);
	TestEqual("Clamped", Pose[0].M[3][0], 2.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimInstanceSkinTest, "System.AnimationCore.AnimInstance.BlendSpaceSkin",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimInstanceSkinTest::RunTest(const FString& Parameters)
{
	const USkeleton Skeleton = MakeTwoBoneSkeleton();
	const UAnimSequence Idle = MakeTranslatedClip("Idle", FVector(0.0f, 1.0f, 0.0f));
	const UAnimSequence Run = MakeTranslatedClip("Run", FVector(0.0f, 2.0f, 0.0f));

	UBlendSpace1D Bs;
	Bs.AddSample(&Idle, 0.0f);
	Bs.AddSample(&Run, 1.0f);

	{
		// Idle input: the child is at its bind pose, so its skin matrix is the identity.
		UAnimInstance& Anim = *NewObject<UAnimInstance>();
		Anim.SetSkeleton(&Skeleton);
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
		Anim.SetSkeleton(&Skeleton);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimInstanceEaseTest, "System.AnimationCore.AnimInstance.EaseBlendInput",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimInstanceEaseTest::RunTest(const FString& Parameters)
{
	const USkeleton Skeleton = MakeTwoBoneSkeleton();
	const UAnimSequence Idle = MakeTranslatedClip("Idle", FVector(0.0f, 1.0f, 0.0f));
	const UAnimSequence Run = MakeTranslatedClip("Run", FVector(0.0f, 2.0f, 0.0f));
	UBlendSpace1D Bs;
	Bs.AddSample(&Idle, 0.0f);
	Bs.AddSample(&Run, 1.0f);

	UAnimInstance& Anim = *NewObject<UAnimInstance>();
	Anim.SetSkeleton(&Skeleton);
	Anim.SetBlendSpace(&Bs);
	Anim.SetLocomotionBlendInterpSpeed(8.0f);
	Anim.SetBlendSpaceInput(1.0f);
	Anim.NativeUpdateAnimation(0.016f);
	TestTrue("Moving toward the target", Anim.GetBlendSpaceInput() > 0.0f && Anim.GetBlendSpaceInput() < 1.0f);
	TestEqual("Target", Anim.GetBlendSpaceInputTarget(), 1.0f, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterAnimJumpTest, "System.AnimationCore.CharacterAnimInstance.JumpStateMachine",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterAnimJumpTest::RunTest(const FString& Parameters)
{
	const USkeleton Skeleton = MakeTwoBoneSkeleton();
	UAnimSequence Idle = MakeTranslatedClip("Idle", FVector(0.0f, 1.0f, 0.0f));
	UAnimSequence Run = MakeTranslatedClip("Run", FVector(0.0f, 2.0f, 0.0f));
	UAnimSequence Jump = MakeTranslatedClip("Jump", FVector(0.0f, 3.0f, 0.0f));
	UAnimSequence Fall = MakeTranslatedClip("Fall", FVector(0.0f, 4.0f, 0.0f));
	UAnimSequence Land = MakeTranslatedClip("Land", FVector(0.0f, 1.5f, 0.0f));
	Jump.bLooping = false;
	Jump.DurationSeconds = 0.2f;
	Fall.bLooping = true;
	Land.bLooping = false;
	Land.DurationSeconds = 0.2f;

	UBlendSpace1D Bs;
	Bs.AddSample(&Idle, 0.0f);
	Bs.AddSample(&Run, 1.0f);

	UCharacterAnimInstance& Anim = *NewObject<UCharacterAnimInstance>();
	Anim.SetSkeleton(&Skeleton);
	Anim.SetBlendSpace(&Bs);
	Anim.SetJumpClips({&Jump, &Fall, &Land});
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterAnimPlayRateTest, "System.AnimationCore.CharacterAnimInstance.JumpPlayRate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterAnimPlayRateTest::RunTest(const FString& Parameters)
{
	// A faster jump play rate finishes the one-shot sooner.
	const USkeleton Skeleton = MakeTwoBoneSkeleton();
	UAnimSequence Idle = MakeTranslatedClip("Idle", FVector(0.0f, 1.0f, 0.0f));
	UAnimSequence Jump = MakeTranslatedClip("Jump", FVector(0.0f, 3.0f, 0.0f));
	UAnimSequence Fall = MakeTranslatedClip("Fall", FVector(0.0f, 4.0f, 0.0f));
	Jump.bLooping = false;
	Jump.DurationSeconds = 1.0f;
	Fall.bLooping = true;

	UBlendSpace1D Bs;
	Bs.AddSample(&Idle, 0.0f);

	UCharacterAnimInstance& Anim = *NewObject<UCharacterAnimInstance>();
	Anim.SetSkeleton(&Skeleton);
	Anim.SetBlendSpace(&Bs);
	Anim.SetJumpClips({&Jump, &Fall, nullptr});
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
