#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "SkeletalAnimation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkeletonFindBoneTest, "System.AnimationCore.Skeleton.FindBoneIndex",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSkeletonFindBoneTest::RunTest(const FString& Parameters)
{
	FReferenceSkeleton Sk;
	Sk.BoneNames = {FName("root"), FName("hips"), FName("spine")};
	Sk.ParentIndices = {INDEX_NONE, 0, 1};
	Sk.InverseBindPose.Init(FMatrix::Identity, 3);

	TestEqual("Bone count", Sk.GetNum(), 3);
	TestEqual("Found", Sk.FindBoneIndex(FName("hips")), 1);
	TestEqual("Missing", Sk.FindBoneIndex(FName("missing")), INDEX_NONE);
	TestEqual("Parent", Sk.GetParentIndex(2), 1);
	TestTrue("Name", Sk.GetBoneName(0) == FName("root"));
	TestEqual("Invalid parent", Sk.GetParentIndex(7), INDEX_NONE);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
