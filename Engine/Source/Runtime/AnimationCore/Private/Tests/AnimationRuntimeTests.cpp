#include "AnimationRuntime.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "SkeletalAnimation.h"

#if WITH_DEV_AUTOMATION_TESTS

// The pose operations of the animation runtime (Docs/PLANS/ps2-shipping.md N25): n-way blends, the layered blend per
// bone, the additive poses of an aim offset and the 2D blend space's triangulation.

namespace
{

	/** root -> pelvis -> (thigh, spine -> hand): a lower body (thigh) and an upper body (spine, hand). */
	FReferenceSkeleton MakeBody()
	{
		FReferenceSkeleton Sk;
		Sk.BoneNames = {FName("root"), FName("pelvis"), FName("thigh"), FName("spine"), FName("hand")};
		Sk.ParentIndices = {INDEX_NONE, 0, 1, 1, 3};
		Sk.RefBonePose.Init(FTransform(FVector(0.0f, 0.0f, 10.0f)), 5);
		Sk.InverseBindPose.Init(FMatrix::Identity, 5);
		return Sk;
	}

	/** Every bone of a pose translated to (Value, 0, 0) and turned by Rotation. */
	TArray<FTransform> MakePose(float Value, const FQuat& Rotation = FQuat::Identity)
	{
		TArray<FTransform> Pose;
		Pose.Init(FTransform(Rotation, FVector(Value, 0.0f, 0.0f)), 5);
		return Pose;
	}

	/** The degrees between two rotations. */
	float AngleBetween(const FQuat& A, const FQuat& B)
	{
		return FMath::RadiansToDegrees(A.AngularDistance(B));
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationRuntimeBlendPosesTest, "System.AnimationCore.Runtime.BlendPosesTogether",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimationRuntimeBlendPosesTest::RunTest(const FString& Parameters)
{
	// Three poses by weight: the translations are the weighted sum, the rotations an n-way normalized lerp that keeps
	// the first pose's hemisphere (a quaternion and its negation are one rotation and do not cancel).
	const FQuat Turn(FVector(0.0f, 0.0f, 1.0f), HALF_PI);
	const TArray<FTransform> A = MakePose(0.0f);
	const TArray<FTransform> B = MakePose(10.0f, Turn);
	TArray<FTransform> C = MakePose(20.0f);
	for (FTransform& Bone : C)
	{
		// The identity as its negation.
		Bone.SetRotation(FQuat(0.0f, 0.0f, 0.0f, -1.0f));
	}
	TArray<FTransform> Out;
	Out.SetNum(5);
	const TArrayView<const FTransform> Poses[3] = {A, B, C};
	const float Weights[3] = {0.25f, 0.5f, 0.25f};
	FAnimationRuntime::BlendPosesTogether(TArrayView<const TArrayView<const FTransform>>(Poses, 3), Weights, Out);
	TestEqual("Weighted translation", Out[2].GetTranslation().X, 10.0f, 1.0e-4f);
	TestEqual("Half the turn (identity twice as heavy as the negated identity cancels nothing)",
		AngleBetween(Out[2].GetRotation(), FQuat::Identity), 45.0f, 0.5f);

	// Two poses: the same as BlendTwoPosesTogether.
	TArray<FTransform> Two;
	Two.SetNum(5);
	FAnimationRuntime::BlendTwoPosesTogether(A, B, 0.5f, Two);
	const float Halves[2] = {0.5f, 0.5f};
	FAnimationRuntime::BlendPosesTogether(TArrayView<const TArrayView<const FTransform>>(Poses, 2), Halves, Out);
	TestTrue("Two poses agree",
		Two[4].GetTranslation().Equals(Out[4].GetTranslation(), 1.0e-4f) &&
			AngleBetween(Two[4].GetRotation(), Out[4].GetRotation()) < 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationRuntimeLayeredBlendTest, "System.AnimationCore.Runtime.LayeredBlendPerBone",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimationRuntimeLayeredBlendTest::RunTest(const FString& Parameters)
{
	// The branch from "spine" takes the spine and the hand, never the pelvis or the thigh: the lower body keeps the
	// base pose (the locomotion) whatever the layer (a montage) does.
	const FReferenceSkeleton Sk = MakeBody();
	TArray<float> Weights;
	Weights.SetNumZeroed(Sk.GetNum());
	FAnimationRuntime::FillBranchBoneWeights(Sk, Sk.FindBoneIndex(FName("spine")), Weights);
	TestTrue("Branch weights",
		Weights[0] == 0.0f && Weights[1] == 0.0f && Weights[2] == 0.0f && Weights[3] == 1.0f && Weights[4] == 1.0f);

	TArray<FTransform> Base = MakePose(1.0f);
	const TArray<FTransform> Layer = MakePose(9.0f, FQuat(FVector(1.0f, 0.0f, 0.0f), HALF_PI));
	FAnimationRuntime::BlendPosesPerBoneFilter(Base, Layer, Weights, 1.0f);
	TestEqual("The thigh keeps the base", Base[2].GetTranslation().X, 1.0f, 1.0e-5f);
	TestTrue("The thigh keeps its rotation", AngleBetween(Base[2].GetRotation(), FQuat::Identity) < 1.0e-3f);
	TestEqual("The pelvis keeps the base", Base[1].GetTranslation().X, 1.0f, 1.0e-5f);
	TestEqual("The spine takes the layer", Base[3].GetTranslation().X, 9.0f, 1.0e-5f);
	TestEqual(
		"The hand takes the layer's rotation", AngleBetween(Base[4].GetRotation(), FQuat::Identity), 90.0f, 0.01f);

	// Half the alpha: halfway on the branch, the rest untouched.
	TArray<FTransform> Half = MakePose(1.0f);
	FAnimationRuntime::BlendPosesPerBoneFilter(Half, Layer, Weights, 0.5f);
	TestEqual("Halfway on the spine", Half[3].GetTranslation().X, 5.0f, 1.0e-4f);
	TestEqual("Still the base on the thigh", Half[2].GetTranslation().X, 1.0f, 1.0e-5f);

	// No branch bone: nothing is in the layer.
	FAnimationRuntime::FillBranchBoneWeights(Sk, INDEX_NONE, Weights);
	TestTrue("No branch", Weights[3] == 0.0f && Weights[4] == 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationRuntimeAdditiveTest, "System.AnimationCore.Runtime.AdditivePose",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimationRuntimeAdditiveTest::RunTest(const FString& Parameters)
{
	// Target - Base added to Base gives the target back at weight 1, half the delta at 0.5, and to another pose the
	// same delta on top of it (an aim offset over any locomotion).
	const FQuat Pitch(FVector(0.0f, 1.0f, 0.0f), -HALF_PI);
	const TArray<FTransform> Base = MakePose(2.0f);
	TArray<FTransform> Additive = MakePose(5.0f, Pitch);
	FAnimationRuntime::ConvertPoseToAdditive(Additive, Base);
	TestEqual("The delta's translation", Additive[0].GetTranslation().X, 3.0f, 1.0e-5f);

	TArray<FTransform> Full = Base;
	FAnimationRuntime::AccumulateAdditivePose(Full, Additive, 1.0f);
	TestEqual("Weight 1: the target", Full[0].GetTranslation().X, 5.0f, 1.0e-4f);
	TestTrue("Weight 1: the target's rotation", AngleBetween(Full[0].GetRotation(), Pitch) < 0.01f);

	TArray<FTransform> Half = Base;
	FAnimationRuntime::AccumulateAdditivePose(Half, Additive, 0.5f);
	TestEqual("Weight 0.5: half the delta", Half[0].GetTranslation().X, 3.5f, 1.0e-4f);
	TestEqual("Weight 0.5: half the turn", AngleBetween(Half[0].GetRotation(), FQuat::Identity), 45.0f, 0.1f);

	// Only the weighted bones move.
	TArray<float> Weights;
	Weights.SetNumZeroed(5);
	Weights[3] = 1.0f;
	TArray<FTransform> Masked = MakePose(7.0f);
	FAnimationRuntime::AccumulateAdditivePose(Masked, Additive, 1.0f, Weights);
	TestEqual("Masked out", Masked[2].GetTranslation().X, 7.0f, 1.0e-5f);
	TestEqual("Masked in: over another pose", Masked[3].GetTranslation().X, 10.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlendSpaceTriangulationTest, "System.AnimationCore.BlendSpace.Triangulation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBlendSpaceTriangulationTest::RunTest(const FString& Parameters)
{
	// A 3 x 3 grid (the corners, the edges' middles and the centre, normalized): 8 triangles, not the overlapping
	// candidates four co-circular points give; the same triangles every time.
	TArray<FVector2D> Grid;
	for (int32 Y = 0; Y < 3; ++Y)
	{
		for (int32 X = 0; X < 3; ++X)
		{
			Grid.Add(FVector2D(0.5f * float(X), 0.5f * float(Y)));
		}
	}
	TArray<FBlendSpaceTriangle> Triangles;
	FBlendSpaceTriangulation::Triangulate(Grid, Triangles);
	TestEqual("Eight triangles", Triangles.Num(), 8);
	TArray<FBlendSpaceTriangle> Again;
	FBlendSpaceTriangulation::Triangulate(Grid, Again);
	bool bSame = Again.Num() == Triangles.Num();
	for (int32 Index = 0; bSame && Index < Triangles.Num(); ++Index)
	{
		bSame = FMemory::Memcmp(&Again[Index], &Triangles[Index], sizeof(FBlendSpaceTriangle)) == 0;
	}
	TestTrue("Deterministic", bSame);

	FBlendSampleData Samples[3];
	// On a sample: that sample alone.
	int32 Count = FBlendSpaceTriangulation::GetWeights(Grid, Triangles, FVector2D(1.0f, 1.0f), Samples);
	TestTrue("A corner", Count == 1 && Samples[0].SampleDataIndex == 8 && Samples[0].TotalWeight == 1.0f);
	Count = FBlendSpaceTriangulation::GetWeights(Grid, Triangles, FVector2D(0.5f, 0.5f), Samples);
	TestTrue("The centre", Count == 1 && Samples[0].SampleDataIndex == 4);
	// On an edge between two samples: half each.
	Count = FBlendSpaceTriangulation::GetWeights(Grid, Triangles, FVector2D(0.25f, 0.0f), Samples);
	TestTrue("An edge",
		Count == 2 && FMath::IsNearlyEqual(Samples[0].TotalWeight, 0.5f, 1.0e-4f) &&
			FMath::IsNearlyEqual(Samples[1].TotalWeight, 0.5f, 1.0e-4f));
	// Inside a triangle: three weights that add up to 1 and rebuild the point.
	const FVector2D Inside(0.3f, 0.1f);
	Count = FBlendSpaceTriangulation::GetWeights(Grid, Triangles, Inside, Samples);
	FVector2D Rebuilt(0.0f, 0.0f);
	float Sum = 0.0f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Rebuilt += Grid[Samples[Index].SampleDataIndex] * Samples[Index].TotalWeight;
		Sum += Samples[Index].TotalWeight;
	}
	TestEqual("Inside: three samples", Count, 3);
	TestEqual("Inside: the weights add up to 1", Sum, 1.0f, 1.0e-5f);
	TestTrue("Inside: they rebuild the input", FVector2D::Distance(Rebuilt, Inside) < 1.0e-4f);
	// Outside the samples: the nearest edge's point.
	Count = FBlendSpaceTriangulation::GetWeights(Grid, Triangles, FVector2D(0.75f, -1.0f), Samples);
	TestTrue("Outside: the nearest edge", Count == 2 && FMath::IsNearlyEqual(Samples[0].TotalWeight, 0.5f, 1.0e-4f));

	// Fewer than three samples, or all in a line: the segments between them.
	const TArray<FVector2D> Line = {FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f)};
	FBlendSpaceTriangulation::Triangulate(Line, Triangles);
	TestEqual("No triangle", Triangles.Num(), 0);
	Count = FBlendSpaceTriangulation::GetWeights(Line, Triangles, FVector2D(0.75f, 0.5f), Samples);
	TestTrue("Along the line", Count == 2 && FMath::IsNearlyEqual(Samples[1].TotalWeight, 0.75f, 1.0e-4f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
