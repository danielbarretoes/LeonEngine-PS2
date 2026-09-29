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

	/** root, and child 1 cm along Y at rest (its bind pose too). */
	USkeleton* MakeTwoBoneSkeleton()
	{
		FReferenceSkeleton Bones;
		Bones.BoneNames = {FName("root"), FName("child")};
		Bones.ParentIndices = {INDEX_NONE, 0};
		Bones.RefBonePose = {FTransform::Identity, FTransform(FVector(0.0f, 1.0f, 0.0f))};
		Bones.InverseBindPose = {FMatrix::Identity, FTranslationMatrix(FVector(0.0f, -1.0f, 0.0f))};
		USkeleton* Skeleton = NewObject<USkeleton>();
		Skeleton->SetReferenceSkeleton(Bones);
		return Skeleton;
	}

	/** A clip whose frames hold the given local translation per bone: Frames[frame][bone]. */
	UAnimSequence* MakeClip(const TArray<TArray<FVector>>& Frames, float Length, float Rate, bool bInLoop = true)
	{
		FRawAnimSequence Raw;
		Raw.SequenceLength = Length;
		Raw.FrameRate = Rate;
		Raw.bLoop = bInLoop;
		const int32 NumBones = Frames.Num() > 0 ? Frames[0].Num() : 0;
		Raw.Tracks.SetNum(NumBones);
		for (const TArray<FVector>& Frame : Frames)
		{
			for (int32 Bone = 0; Bone < NumBones; ++Bone)
			{
				Raw.Tracks[Bone].PosKeys.Add(Frame[Bone]);
				Raw.Tracks[Bone].RotKeys.Add(FQuat::Identity);
				Raw.Tracks[Bone].ScaleKeys.Add(FVector::OneVector);
			}
		}
		UAnimSequence* Clip = NewObject<UAnimSequence>();
		(void)Clip->SetFromRawAnimSequence(Raw);
		return Clip;
	}

	/** A one-frame two-bone clip with the child at ChildLocalTranslation. */
	UAnimSequence* MakeTranslatedClip(const FVector& ChildLocalTranslation)
	{
		return MakeClip({{FVector::ZeroVector, ChildLocalTranslation}}, 1.0f, 1.0f);
	}

	UAnimSequence* MakeTwoBoneRestClip()
	{
		return MakeTranslatedClip(FVector(0.0f, 1.0f, 0.0f));
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimSequenceLoopTest, "System.Engine.Animation.Sequence.Loop",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimSequenceLoopTest::RunTest(const FString& Parameters)
{
	UAnimSequence& Clip = *MakeClip({{FVector::ZeroVector}, {FVector(1.0f, 0.0f, 0.0f)}}, 2.0f, 1.0f);
	TestEqual("Frames", Clip.GetNumberOfFrames(), 2);

	TArray<FTransform> Pose;
	Clip.GetBonePose(0.0f, Pose);
	TestEqual("Bones", Pose.Num(), 1);
	TestEqual("Start", Pose[0].GetTranslation().X, 0.0f, 1.0e-4f);

	Clip.GetBonePose(2.0f, Pose); // wraps to the start
	TestEqual("Wrapped", Pose[0].GetTranslation().X, 0.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimSequenceOneShotTest, "System.Engine.Animation.Sequence.OneShot",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimSequenceOneShotTest::RunTest(const FString& Parameters)
{
	// A one-shot clip clamps to its last frame and reports when it is finished.
	UAnimSequence& Clip =
		*MakeClip({{FVector::ZeroVector}, {FVector(2.0f, 0.0f, 0.0f)}}, 1.0f, 1.0f, /*bInLoop =*/false);

	TestFalse("Not finished at the start", Clip.IsFinished(0.0f));
	TestTrue("Finished at the end", Clip.IsFinished(1.0f));

	TArray<FTransform> Pose;
	Clip.GetBonePose(5.0f, Pose);
	TestEqual("Clamped", Pose[0].GetTranslation().X, 2.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimSequenceLerpTest, "System.Engine.Animation.Sequence.LerpMidFrame",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimSequenceLerpTest::RunTest(const FString& Parameters)
{
	// Halfway between two frames the translation is halfway too, in the bone's local space.
	UAnimSequence& Clip = *MakeClip({{FVector::ZeroVector}, {FVector(2.0f, 0.0f, 0.0f)}}, 1.0f, 1.0f);

	TArray<FTransform> Pose;
	Clip.GetBonePose(0.5f, Pose);
	TestEqual("Bones", Pose.Num(), 1);
	TestEqual("Translation", Pose[0].GetTranslation().X, 1.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendSpace1DEvaluateTest, "System.Engine.Animation.BlendSpace1D.Evaluate",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBlendSpace1DEvaluateTest::RunTest(const FString& Parameters)
{
	UAnimSequence* Idle = MakeTwoBoneRestClip();
	UAnimSequence* Run = MakeTwoBoneRestClip();

	UBlendSpace1D& Bs = *NewObject<UBlendSpace1D>();
	TestEqual("Default axis min", Bs.GetBlendParameter(0).Min, 0.0f, 1.0e-6f);
	TestEqual("Default axis max", Bs.GetBlendParameter(0).Max, 1.0f, 1.0e-6f);
	Bs.AddSample(Idle, 0.0f);
	Bs.AddSample(Run, 1.0f);

	FBlendSampleDataArray Samples;
	auto IsOnly = [&](int32 Index)
	{ return Samples.Num() == 1 && Samples[0].SampleDataIndex == Index && Samples[0].TotalWeight == 1.0f; };

	// At idle.
	Bs.GetSamplesFromBlendInput(FVector(0.0f, 0.0f, 0.0f), Samples);
	TestTrue("Idle: the idle sample alone", IsOnly(0));

	// Mid blend.
	Bs.GetSamplesFromBlendInput(FVector(0.5f, 0.0f, 0.0f), Samples);
	TestTrue(
		"Mid: idle and run", Samples.Num() == 2 && Samples[0].SampleDataIndex == 0 && Samples[1].SampleDataIndex == 1);
	TestEqual("Mid: the run's weight", Samples.Num() == 2 ? Samples[1].TotalWeight : -1.0f, 0.5f, 1.0e-5f);

	// At run.
	Bs.GetSamplesFromBlendInput(FVector(1.0f, 0.0f, 0.0f), Samples);
	TestTrue("Run: the run sample alone", IsOnly(1));

	// A quarter of the way: 0.75 idle, 0.25 run.
	Bs.GetSamplesFromBlendInput(FVector(0.25f, 0.0f, 0.0f), Samples);
	TestTrue("Quarter: weights", Samples.Num() == 2 && FMath::IsNearlyEqual(Samples[0].TotalWeight, 0.75f, 1.0e-5f));

	// Clamped below and above the axis.
	Bs.GetSamplesFromBlendInput(FVector(-2.0f, 0.0f, 0.0f), Samples);
	TestTrue("Below the axis: idle", IsOnly(0));
	Bs.GetSamplesFromBlendInput(FVector(3.0f, 0.0f, 0.0f), Samples);
	TestTrue("Above the axis: run", IsOnly(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendSpace1DEmptyTest, "System.Engine.Animation.BlendSpace1D.Empty",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBlendSpace1DEmptyTest::RunTest(const FString& Parameters)
{
	UBlendSpace1D& Bs = *NewObject<UBlendSpace1D>();
	FBlendSampleDataArray Samples;
	Samples.Add({3, 1.0f});
	Bs.GetSamplesFromBlendInput(FVector(0.5f, 0.0f, 0.0f), Samples);
	TestEqual("No samples", Samples.Num(), 0);
	TestFalse("A null clip is not a sample", Bs.AddSample(nullptr, 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimInstanceNoSkeletonTest, "System.Engine.Animation.AnimInstance.NoSkeleton",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimInstanceNoSkeletonTest::RunTest(const FString& Parameters)
{
	UAnimInstance& Anim = *NewObject<UAnimInstance>();
	Anim.UpdateAnimation(0.016f);
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
		// Idle input: the child is 1 cm along Y, its bind pose, so its skin matrix is the identity.
		UAnimInstance& Anim = *NewObject<UAnimInstance>();
		Anim.SetSkeleton(Skeleton);
		Anim.SetBlendSpace(&Bs);
		Anim.SetLocomotionBlendInterpSpeed(0.0f); // snap for unit tests
		Anim.SetBlendSpaceInput(0.0f);
		Anim.UpdateAnimation(0.016f);
		TArray<FMatrix> Skin;
		Anim.GetSkinMatrices(Skin);
		TestEqual("Idle: bones", Skin.Num(), 2);
		TestEqual("Idle: child at bind", Skin[1].M[3][1], 0.0f, 1.0e-3f);
		TArray<FTransform> Pose;
		Anim.EvaluatePose(Pose);
		TestEqual("Idle: a local pose", Pose.Num(), 2);
		TestEqual("Idle: one sample", Anim.GetLocomotionSamples().Num(), 1);
	}
	{
		// Mid input blends.
		UAnimInstance& Anim = *NewObject<UAnimInstance>();
		Anim.SetSkeleton(Skeleton);
		Anim.SetBlendSpace(&Bs);
		Anim.SetLocomotionBlendInterpSpeed(0.0f);
		Anim.SetBlendSpaceInput(0.5f);
		Anim.UpdateAnimation(0.016f);
		TestTrue("Mid: two samples, half each",
			Anim.GetLocomotionSamples().Num() == 2 &&
				FMath::IsNearlyEqual(Anim.GetLocomotionSamples()[1].TotalWeight, 0.5f, 1.0e-5f));
		TArray<FMatrix> Skin;
		Anim.GetSkinMatrices(Skin);
		TestEqual("Mid: bones", Skin.Num(), 2);
		// Blended in local space: the child halfway between 1 and 2 cm, so 0.5 cm past its bind.
		TestEqual("Mid: the child halfway", Skin[1].M[3][1], 0.5f, 1.0e-3f);
	}
	{
		// Without clips the skeleton's reference pose: the bind pose here.
		UAnimInstance& Anim = *NewObject<UAnimInstance>();
		Anim.SetSkeleton(Skeleton);
		Anim.UpdateAnimation(0.016f);
		TArray<FMatrix> Skin;
		Anim.GetSkinMatrices(Skin);
		TestTrue("Reference pose", Skin.Num() == 2 && Skin[1].Equals(FMatrix::Identity, 1.0e-4f));
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
	Anim.UpdateAnimation(0.016f);
	TestTrue("Moving toward the target", Anim.GetBlendSpaceInput().X > 0.0f && Anim.GetBlendSpaceInput().X < 1.0f);
	TestEqual("Target", Anim.GetBlendSpaceInputTarget().X, 1.0f, 1.0e-5f);
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
	Anim.UpdateAnimation(0.016f);
	TestTrue("Jump start", Anim.GetJumpState() == EAnimJumpState::JumpStart);
	TestTrue("Crossfading", Anim.GetCrossfadeAlpha() < 1.0f);

	Anim.SetMovementState(true, -1.0f, false);
	Anim.UpdateAnimation(0.016f);
	TestTrue("Falling", Anim.GetJumpState() == EAnimJumpState::FallLoop);

	Anim.SetMovementState(false, 0.0f, true);
	Anim.UpdateAnimation(0.016f);
	TestTrue("Landing", Anim.GetJumpState() == EAnimJumpState::Land);

	for (int32 I = 0; I < 20; ++I)
	{
		Anim.SetMovementState(false, 0.0f, false);
		Anim.UpdateAnimation(0.05f);
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
	Anim.UpdateAnimation(0.0f);
	TestTrue("Jump start", Anim.GetJumpState() == EAnimJumpState::JumpStart);

	Anim.SetMovementState(true, 5.0f, false);
	Anim.UpdateAnimation(0.3f);
	TestTrue("Falling after 0.3 s at 4x", Anim.GetJumpState() == EAnimJumpState::FallLoop);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterAnimCrouchTest, "System.Engine.Animation.CharacterAnimInstance.Crouch",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterAnimCrouchTest::RunTest(const FString& Parameters)
{
	// The crouched locomotion (N27): crouching crossfades from the standing space to the crouched one over the
	// crossfade, the notifies come from the space playing only, and standing up fades back.
	UAnimSequence* Stand = MakeTranslatedClip(FVector(0.0f, 2.0f, 0.0f));
	Stand->AddNotify(TEXT("Footstep_L"), 0.5f);
	UAnimSequence* Crouched = MakeTranslatedClip(FVector(0.0f, 1.0f, 0.0f));
	UBlendSpace1D& StandSpace = *NewObject<UBlendSpace1D>();
	StandSpace.AddSample(Stand, 0.0f);
	UBlendSpace1D& CrouchSpace = *NewObject<UBlendSpace1D>();
	CrouchSpace.AddSample(Crouched, 0.0f);

	UCharacterAnimInstance& Anim = *NewObject<UCharacterAnimInstance>();
	Anim.SetSkeleton(MakeTwoBoneSkeleton());
	Anim.SetBlendSpace(&StandSpace);
	Anim.SetCrouchBlendSpace(&CrouchSpace);
	Anim.SetCrossfadeDuration(0.2f);
	TArray<FTransform> Pose;
	auto ChildY = [&]()
	{
		Anim.EvaluatePose(Pose);
		return Pose.Num() == 2 ? Pose[1].GetTranslation().Y : -1.0f;
	};

	Anim.UpdateAnimation(0.1f);
	TestEqual("Standing", ChildY(), 2.0f, 1.0e-3f);

	Anim.SetCrouched(true);
	Anim.UpdateAnimation(0.1f);
	TestEqual("Half way down", Anim.GetCrouchAlpha(), 0.5f, 1.0e-4f);
	TestEqual("A blend of both", ChildY(), 1.5f, 1.0e-3f);
	Anim.UpdateAnimation(0.2f);
	TestEqual("Crouched", ChildY(), 1.0f, 1.0e-3f);
	// Crossing 0.5 s crouched fires nothing: the standing clip's footstep is not playing.
	Anim.UpdateAnimation(0.4f);
	TestEqual("No footstep while crouched", Anim.GetNumNotifiesFiredLastUpdate(), 0);

	Anim.SetCrouched(false);
	Anim.UpdateAnimation(0.3f);
	TestEqual("Standing again", ChildY(), 2.0f, 1.0e-3f);
	TestTrue("The standing space plays", Anim.GetCrouchAlpha() == 0.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
