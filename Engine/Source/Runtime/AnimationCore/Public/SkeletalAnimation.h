#pragma once

// The plain skeletal data below the Engine assets (USkeleton, USkeletalMesh, UAnimSequence): what the FBX importer
// (MeshUtilities) produces and the assets keep. UE declares the reference skeleton in Engine's ReferenceSkeleton.h and
// the raw tracks in AnimTypes.h; Leon keeps them here, where the importer can reach them without the Engine module.

#include "CoreMinimal.h"

constexpr int32 MaxSkinBones = 96;
constexpr int32 MaxBoneInfluences = 4;

/**
 * Bone matrices (inverse bind, sampled poses, skin) are FMatrix values in UE's row-vector convention and in the
 * engine world (the FBX import converts them with FImportCoordinateConversion), uploaded to the shaders as they are.
 * The skin matrix is InverseBind * BoneWorld.
 */
struct ANIMATIONCORE_API FSkeletalVertex
{
	FVector Position = FVector::ZeroVector;
	FVector Normal = FVector(0.0f, 0.0f, 1.0f);
	FVector2D TexCoord = FVector2D::ZeroVector;
	FVector4 Tangent = FVector4(1.0f, 0.0f, 0.0f, 1.0f);
	FIntVector4 BoneIndices = FIntVector4(0);
	FVector4 BoneWeights = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

	/** Every field in order (the payload of USkeletalMesh's bulk data). */
	friend ANIMATIONCORE_API FArchive& operator<<(FArchive& Ar, FSkeletalVertex& Vertex);
};

static_assert(sizeof(FSkeletalVertex) == 80, "FSkeletalVertex is uploaded as an 80-byte interleaved vertex");

/**
 * A skeleton's bones (UE: FReferenceSkeleton): names, parents and the inverse bind pose (cluster geometry_to_bone).
 * UE keeps the reference pose as bone transforms and the inverse bind matrices on the mesh; Leon keeps the inverse
 * bind pose here, with the bones, as the FBX import produces it.
 */
struct ANIMATIONCORE_API FReferenceSkeleton
{
	TArray<FName> BoneNames;
	/** INDEX_NONE for a root. */
	TArray<int32> ParentIndices;
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

	/** The names (through the archive, so a package stores them in its name table), the parents and the pose. */
	friend ANIMATIONCORE_API FArchive& operator<<(FArchive& Ar, FReferenceSkeleton& Skeleton);
};

/**
 * One bone's keys, one per frame (UE: FRawAnimSequenceTrack). Leon's keys are the bone's model-space matrix
 * (node_to_world) at the frame's time, as the FBX import bakes them; UE's are local position, rotation and scale keys.
 */
struct ANIMATIONCORE_API FRawAnimSequenceTrack
{
	TArray<FMatrix> Keys;

	friend ANIMATIONCORE_API FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrack& Track);
};

/**
 * A clip as the FBX import bakes it (Leon; UE: the raw data an animation factory gives a UAnimSequence): one track per
 * bone of the skeleton it was baked against, each with the same number of keys.
 */
struct ANIMATIONCORE_API FRawAnimSequence
{
	FName Name;
	/** Seconds (UE: UAnimSequenceBase::SequenceLength). */
	float SequenceLength = 1.0f;
	/** Keys per second. */
	float FrameRate = 30.0f;
	/** False for a one-shot clip that holds its last frame (UE: bLoop). */
	bool bLoop = true;
	TArray<FRawAnimSequenceTrack> Tracks;

	/** Keys of each track (UE: GetNumberOfFrames). */
	[[nodiscard]] int32 GetNumberOfFrames() const
	{
		return Tracks.Num() > 0 ? Tracks[0].Keys.Num() : 0;
	}
};

/** A skinned mesh as the FBX import produces it (UE: FSkeletalMeshImportData), in the engine world. */
struct ANIMATIONCORE_API FSkeletalMeshData
{
	FReferenceSkeleton RefSkeleton;
	TArray<FSkeletalVertex> Vertices;
	TArray<uint32> Indices;
	FVector LocalMin = FVector::ZeroVector;
	FVector LocalMax = FVector::ZeroVector;
	/** The clip of the file's first animation stack, baked against RefSkeleton. */
	FRawAnimSequence EmbeddedAnim;

	[[nodiscard]] bool IsEmpty() const
	{
		return Vertices.Num() == 0 || Indices.Num() == 0;
	}
};
