#pragma once

#include "CoreMinimal.h"
#include "SkeletalAnimation.h"

/**
 * Pose helpers (UE: FAnimationRuntime, Engine's AnimationRuntime.h): a pose is one local transform per bone of a
 * skeleton; it is blended in local space and turned into component-space matrices once, when it is final.
 *
 * The poses are views, so the caller decides where they live: the anim instances keep their temporaries on the frame's
 * stack (FMemStack, Docs/PLANS/ps2-shipping.md N25) and nothing here allocates. Every pose of one call has the same
 * number of bones.
 */
struct ANIMATIONCORE_API FAnimationRuntime
{
	/**
	 * OutPose = A blended toward B by WeightOfB (0: A, 1: B), bone by bone in local space: translations and scales
	 * lerped, rotations by a normalized lerp along the shortest arc (UE: BlendTwoPosesTogether). OutPose may be A or B.
	 */
	static void BlendTwoPosesTogether(TArrayView<const FTransform> A, TArrayView<const FTransform> B, float WeightOfB,
		TArrayView<FTransform> OutPose);

	/**
	 * OutPose = the weighted sum of NumPoses poses (UE: BlendPosesTogether): translations and scales summed by weight,
	 * rotations summed on the first pose's hemisphere and normalized (an n-way normalized lerp). The weights should add
	 * up to 1. OutPose must not be one of the sources.
	 */
	static void BlendPosesTogether(TArrayView<const TArrayView<const FTransform>> SourcePoses, const float* Weights,
		TArrayView<FTransform> OutPose);

	/**
	 * Layered blend per bone (UE: BlendPosesPerBoneFilter, one layer): each bone of InOutBasePose moves toward
	 * BlendPose by Alpha times its weight in BoneWeights (0 keeps the base, 1 takes the blend pose).
	 */
	static void BlendPosesPerBoneFilter(TArrayView<FTransform> InOutBasePose, TArrayView<const FTransform> BlendPose,
		TArrayView<const float> BoneWeights, float Alpha);

	/**
	 * The weights of a layered blend from a branch bone (UE: FInputBlendPose's BranchFilters, blend depth 0): 1 for
	 * BranchBoneIndex and every bone under it, 0 for the others (all 0 for INDEX_NONE). OutBoneWeights has one entry a
	 * bone of Skeleton.
	 */
	static void FillBranchBoneWeights(
		const FReferenceSkeleton& Skeleton, int32 BranchBoneIndex, TArrayView<float> OutBoneWeights);

	/**
	 * Turns a full pose into an additive one against BasePose, in each bone's local space (UE: ConvertPoseToAdditive):
	 * the rotation becomes Target * Base^-1, the translation Target - Base, the scale Target / Base.
	 */
	static void ConvertPoseToAdditive(TArrayView<FTransform> InOutTargetPose, TArrayView<const FTransform> BasePose);

	/**
	 * Adds an additive pose to InOutBasePose with Weight (UE: AccumulateAdditivePose, local space): each bone's delta
	 * scaled toward the identity by Weight times its entry in BoneWeights (every bone at Weight when BoneWeights is
	 * empty), then applied: the rotation Delta * Base, the translation Base + Delta, the scale Base * Delta.
	 */
	static void AccumulateAdditivePose(TArrayView<FTransform> InOutBasePose, TArrayView<const FTransform> AdditivePose,
		float Weight, TArrayView<const float> BoneWeights = TArrayView<const float>());

	/**
	 * The component-space matrix of every bone: its local transform's matrix times its parent's (UE:
	 * FillUpComponentSpaceTransforms; Leon keeps matrices). LocalPose has one transform per bone of Skeleton, whose
	 * parents come before their children. OutComponentSpace is resized, reusing its memory.
	 */
	static void FillUpComponentSpaceTransforms(
		const FReferenceSkeleton& Skeleton, TArrayView<const FTransform> LocalPose, TArray<FMatrix>& OutComponentSpace);

	/**
	 * The skin matrices of a pose: InverseBindPose[Bone] * ComponentSpace[Bone], what moves a bind-pose vertex to the
	 * pose (Leon; UE's GetCurrentRefToLocalMatrices). OutSkin is resized, reusing its memory.
	 */
	static void GetSkinMatrices(
		const FReferenceSkeleton& Skeleton, TArrayView<const FMatrix> ComponentSpace, TArray<FMatrix>& OutSkin);
};

/** A triangle of a 2D blend space's samples (Leon; UE: FTriangle of the editor's FDelaunayTriangleGenerator). */
struct ANIMATIONCORE_API FBlendSpaceTriangle
{
	int32 Indices[3] = {0, 0, 0};
};

/** A sample of a blend space and its weight in a blend (UE: FBlendSampleData, trimmed). */
struct ANIMATIONCORE_API FBlendSampleData
{
	int32 SampleDataIndex = INDEX_NONE;
	float TotalWeight = 0.0f;
};

/**
 * The 2D blend space's interpolation (Leon; UE 4.27 triangulates its samples in the editor, FDelaunayTriangleGenerator,
 * and bakes the triangles' weights into a grid): the samples are triangulated once (Delaunay, on the axes normalized to
 * their ranges), and an input takes the barycentric weights of the triangle it falls in, or, outside every triangle,
 * the weights of the nearest point on the triangles' edges. Deterministic: the same samples give the same triangles.
 */
struct ANIMATIONCORE_API FBlendSpaceTriangulation
{
	/**
	 * Delaunay triangles of Points (already normalized): every triple whose circumcircle holds no other point strictly
	 * inside, taken in index order and skipping those that overlap one already taken (four points on a circle, a grid's
	 * cells, give two triangles, not four). Collinear triples are left out. OutTriangles is reset.
	 */
	static void Triangulate(TArrayView<const FVector2D> Points, TArray<FBlendSpaceTriangle>& OutTriangles);

	/**
	 * The samples around Query and their weights (adding up to 1), at most three, into OutSamples (reset): the
	 * containing triangle's barycentric weights, else the nearest edge point's two, else (no triangle: fewer than three
	 * samples not in a line) the nearest segment of consecutive points' two, or the one point. Weights below 1e-5 are
	 * dropped. Returns the number of samples.
	 */
	static int32 GetWeights(TArrayView<const FVector2D> Points, TArrayView<const FBlendSpaceTriangle> Triangles,
		const FVector2D& Query, FBlendSampleData OutSamples[3]);
};
