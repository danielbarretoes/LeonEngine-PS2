#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "SkeletalAnimation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkeletonFindBoneTest, "System.AnimationCore.Skeleton.FindBoneIndex",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSkeletonFindBoneTest::RunTest(const FString& Parameters)
{
	USkeleton Sk;
	Sk.BoneNames = {FName("root"), FName("hips"), FName("spine")};
	Sk.ParentIndices = {INDEX_NONE, 0, 1};
	Sk.InverseBindPose.Init(FMatrix::Identity, 3);

	TestEqual("Bone count", Sk.BoneCount(), 3);
	TestEqual("Found", Sk.FindBoneIndex(FName("hips")), 1);
	TestEqual("Missing", Sk.FindBoneIndex(FName("missing")), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimSequenceLerpTest, "System.AnimationCore.Sequence.LerpMidFrame",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimSequenceLerpTest::RunTest(const FString& Parameters)
{
	// Halfway between two frames the translation is halfway too.
	UAnimSequence Clip;
	Clip.DurationSeconds = 1.0f;
	Clip.FramesPerSecond = 1.0f;
	Clip.LocalPoseFrames.SetNum(2);
	Clip.LocalPoseFrames[0] = {FMatrix::Identity};
	Clip.LocalPoseFrames[1] = {FTranslationMatrix(FVector(2.0f, 0.0f, 0.0f))};

	TArray<FMatrix> Pose;
	Clip.SampleLocalPose(0.5f, Pose);
	TestEqual("Bones", Pose.Num(), 1);
	TestEqual("Translation", Pose[0].M[3][0], 1.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimInstanceNoSkeletonTest, "System.AnimationCore.AnimInstance.NoSkeleton",
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

#endif // WITH_DEV_AUTOMATION_TESTS
