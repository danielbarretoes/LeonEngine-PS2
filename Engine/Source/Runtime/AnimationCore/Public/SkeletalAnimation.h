#pragma once

// The plain skeletal data below the Engine assets (USkeleton, USkeletalMesh, UAnimSequence): what the glTF importer
// (MeshUtilities) produces and the assets keep. UE declares the reference skeleton in Engine's ReferenceSkeleton.h, the
// skin weights in SkeletalMeshTypes.h and the raw tracks in AnimTypes.h; Leon keeps them here, where the importer and
// the EE's runtime reach them without the Engine module.

#include "CoreMinimal.h"

/** The most bones a skeleton may have: a bone index fits a byte, and a pose of 96 bones stays cheap on the EE. */
constexpr int32 MaxSkinBones = 96;

/**
 * The bones that move a vertex (UE: MAX_TOTAL_INFLUENCES, 4 or 8 there). Two is the PS2's budget: the skinned LPS2
 * layout gives each vertex two palette indices and two weights (Docs/ASSET_FORMATS.md, skinned meshes); the importer
 * keeps the two largest weights and renormalizes them.
 */
constexpr int32 MaxBoneInfluences = 2;

/**
 * The bones and weights of a skinned vertex (UE: FSkinWeightInfo): InfluenceBones index the skeleton's bones and
 * InfluenceWeights are 1/255 steps that add up to exactly 255. An unused second influence has a weight of 0 and repeats
 * the first bone.
 */
struct ANIMATIONCORE_API FSkinWeightInfo
{
	uint8 InfluenceBones[MaxBoneInfluences] = {0, 0};
	uint8 InfluenceWeights[MaxBoneInfluences] = {255, 0};

	friend ANIMATIONCORE_API FArchive& operator<<(FArchive& Ar, FSkinWeightInfo& Info);
};

/**
 * A skeleton's bones (UE: FReferenceSkeleton): names, parents, the reference pose (each bone's local transform, UE's
 * RefBonePose) and the inverse bind pose (UE keeps that on the skeletal mesh). A parent always comes before its
 * children, so a pose is built from the first bone to the last in one pass. Transforms and matrices are in the engine
 * world (the importer converts them): a bone's component-space matrix is its local transform's matrix times its
 * parent's, and a vertex is skinned by InverseBindPose[Bone] * ComponentSpace[Bone] (UE's row-vector order).
 */
struct ANIMATIONCORE_API FReferenceSkeleton
{
	TArray<FName> BoneNames;
	/** INDEX_NONE for a root; else a smaller index. */
	TArray<int32> ParentIndices;
	/** Each bone's local transform at rest (UE: GetRefBonePose); what a bone no animation moves keeps. */
	TArray<FTransform> RefBonePose;
	/** From the mesh's space to each bone's space at bind time (glTF: inverseBindMatrices). */
	TArray<FMatrix> InverseBindPose;

	/** Number of bones (UE: GetNum). */
	[[nodiscard]] int32 GetNum() const
	{
		return BoneNames.Num();
	}
	/** The bone called InName, or INDEX_NONE (UE: FindBoneIndex). */
	[[nodiscard]] int32 FindBoneIndex(FName InName) const;
	/** UE: GetBoneName; NAME_None for an invalid index. */
	[[nodiscard]] FName GetBoneName(int32 BoneIndex) const;
	/** UE: GetParentIndex; INDEX_NONE for a root or an invalid index. */
	[[nodiscard]] int32 GetParentIndex(int32 BoneIndex) const;

	/**
	 * True when every array has one entry per bone, there are at most MaxSkinBones, the names are unique and every
	 * parent comes before its child (Leon).
	 */
	[[nodiscard]] bool IsValid() const;

	/** True when Other has the same bones: names, order and parents (what an animation or a mesh must match). */
	[[nodiscard]] bool HasSameBones(const FReferenceSkeleton& Other) const;

	/** The names (through the archive, so a package stores them in its name table), parents, pose and bind pose. */
	friend ANIMATIONCORE_API FArchive& operator<<(FArchive& Ar, FReferenceSkeleton& Skeleton);
};

/**
 * One bone's keys, one per frame, in the bone's local space (UE: FRawAnimSequenceTrack): the translation (centimetres),
 * the rotation and the scale. A channel with a single key holds it for the whole clip.
 */
struct ANIMATIONCORE_API FRawAnimSequenceTrack
{
	TArray<FVector> PosKeys;
	TArray<FQuat> RotKeys;
	TArray<FVector> ScaleKeys;

	/** The track's transform at a frame (its last key past the end of a channel). */
	[[nodiscard]] FTransform GetKey(int32 Frame) const;
};

/**
 * A named event on a clip's timeline as its source authors it (Leon; UE: the FAnimNotifyEvent the editor adds): glTF
 * animation extras `{"notifies": [{"name": "Footstep_L", "time": 0.25}]}` (Docs/ASSET_FORMATS.md, animation notifies).
 */
struct ANIMATIONCORE_API FRawAnimNotify
{
	FName NotifyName;
	/** Seconds from the clip's start, 0 to its length. */
	float Time = 0.0f;
};

/**
 * A clip as the importer samples it (Leon; UE: the raw data an animation factory gives a UAnimSequence): one track per
 * bone of the skeleton it was sampled against, keys at FrameRate. UAnimSequence compresses it (FAnimCompression).
 */
struct ANIMATIONCORE_API FRawAnimSequence
{
	/** The clip's notifies, in the order of their times (UAnimSequence keeps them as FAnimNotifyEvents). */
	TArray<FRawAnimNotify> Notifies;

	FName Name;
	/** Seconds (UE: UAnimSequenceBase::SequenceLength): (frames - 1) / FrameRate. */
	float SequenceLength = 1.0f;
	/** Keys per second: 30 for every imported clip. */
	float FrameRate = 30.0f;
	/** False for a one-shot clip that holds its last frame (UE: bLoop). */
	bool bLoop = true;
	TArray<FRawAnimSequenceTrack> Tracks;

	/** The most keys a channel has (UE: GetNumberOfFrames). */
	[[nodiscard]] int32 GetNumberOfFrames() const;
};
