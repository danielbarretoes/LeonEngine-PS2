#include "AnimationRuntime.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "SkeletalAnimation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** root -> hips -> spine, each 10 cm above its parent at rest. */
	FReferenceSkeleton MakeSpine()
	{
		FReferenceSkeleton Sk;
		Sk.BoneNames = {FName("root"), FName("hips"), FName("spine")};
		Sk.ParentIndices = {INDEX_NONE, 0, 1};
		Sk.RefBonePose.Init(FTransform(FVector(0.0f, 0.0f, 10.0f)), 3);
		for (int32 Bone = 0; Bone < 3; ++Bone)
		{
			Sk.InverseBindPose.Add(FTranslationMatrix(FVector(0.0f, 0.0f, -10.0f * float(Bone + 1))));
		}
		return Sk;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkeletonFindBoneTest, "System.AnimationCore.Skeleton.FindBoneIndex",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSkeletonFindBoneTest::RunTest(const FString& Parameters)
{
	const FReferenceSkeleton Sk = MakeSpine();
	TestEqual("Bone count", Sk.GetNum(), 3);
	TestEqual("Found", Sk.FindBoneIndex(FName("hips")), 1);
	TestEqual("Missing", Sk.FindBoneIndex(FName("missing")), INDEX_NONE);
	TestEqual("Parent", Sk.GetParentIndex(2), 1);
	TestTrue("Name", Sk.GetBoneName(0) == FName("root"));
	TestEqual("Invalid parent", Sk.GetParentIndex(7), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkeletonValidityTest, "System.AnimationCore.Skeleton.ValidityAndMatching",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSkeletonValidityTest::RunTest(const FString& Parameters)
{
	// A parent comes before its child, names are unique, every array has a bone's entry; another skeleton matches only
	// with the same names, order and parents.
	const FReferenceSkeleton Sk = MakeSpine();
	TestTrue("Valid", Sk.IsValid());
	FReferenceSkeleton ChildFirst = Sk;
	ChildFirst.ParentIndices = {INDEX_NONE, 2, 0};
	TestFalse("A parent after its child", ChildFirst.IsValid());
	FReferenceSkeleton Twice = Sk;
	Twice.BoneNames[2] = FName("hips");
	TestFalse("A name twice", Twice.IsValid());
	FReferenceSkeleton Short = Sk;
	Short.RefBonePose.SetNum(2);
	TestFalse("A missing pose", Short.IsValid());

	TestTrue("The same bones", Sk.HasSameBones(MakeSpine()));
	FReferenceSkeleton Renamed = Sk;
	Renamed.BoneNames[1] = FName("pelvis");
	TestFalse("Another name", Sk.HasSameBones(Renamed));
	FReferenceSkeleton Reparented = Sk;
	Reparented.ParentIndices[2] = 0;
	TestFalse("Another parent", Sk.HasSameBones(Reparented));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationRuntimePoseTest, "System.AnimationCore.AnimationRuntime.PoseAndSkin",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimationRuntimePoseTest::RunTest(const FString& Parameters)
{
	// The reference pose chains the bones' local transforms: at rest a bone is where its inverse bind pose undoes, so
	// every skin matrix is the identity. A local rotation of the root turns its children with it.
	const FReferenceSkeleton Sk = MakeSpine();
	TArray<FMatrix> ComponentSpace;
	FAnimationRuntime::FillUpComponentSpaceTransforms(Sk, Sk.RefBonePose, ComponentSpace);
	TestTrue("The spine at 30 cm", FVector(ComponentSpace[2].GetOrigin()).Equals(FVector(0.0f, 0.0f, 30.0f), 1.0e-4f));
	TArray<FMatrix> Skin;
	FAnimationRuntime::GetSkinMatrices(Sk, ComponentSpace, Skin);
	for (int32 Bone = 0; Bone < 3; ++Bone)
	{
		TestTrue(*FString::Printf("Bone %d at bind", Bone), Skin[Bone].Equals(FMatrix::Identity, 1.0e-4f));
	}

	TArray<FTransform> Turned = Sk.RefBonePose;
	Turned[0].SetRotation(FQuat(FVector(1.0f, 0.0f, 0.0f), HALF_PI));
	FAnimationRuntime::FillUpComponentSpaceTransforms(Sk, Turned, ComponentSpace);
	const FVector Spine = ComponentSpace[2].GetOrigin();
	TestTrue("The children turned with the root", FMath::Abs(Spine.Z - 10.0f) < 1.0e-3f && FMath::Abs(Spine.Y) > 19.9f);

	// Halfway between the two poses: translations lerped, rotations halfway along the shorter arc.
	TArray<FTransform> Blended;
	Blended.SetNum(Sk.GetNum());
	FAnimationRuntime::BlendTwoPosesTogether(Sk.RefBonePose, Turned, 0.5f, Blended);
	TestEqual("Bones", Blended.Num(), 3);
	TestEqual("45 degrees", FMath::RadiansToDegrees(Blended[0].GetRotation().GetAngle()), 45.0f, 1.0e-2f);
	TestTrue("Unit rotation", Blended[0].GetRotation().IsNormalized());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
