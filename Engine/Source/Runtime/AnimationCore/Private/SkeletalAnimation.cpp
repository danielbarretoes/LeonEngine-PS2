#include "SkeletalAnimation.h"

FArchive& operator<<(FArchive& Ar, FSkeletalVertex& Vertex)
{
	Ar << Vertex.Position << Vertex.Normal << Vertex.TexCoord << Vertex.Tangent;
	Ar << Vertex.BoneIndices.X << Vertex.BoneIndices.Y << Vertex.BoneIndices.Z << Vertex.BoneIndices.W;
	Ar << Vertex.BoneWeights;
	return Ar;
}

int32 FReferenceSkeleton::FindBoneIndex(FName InName) const
{
	return BoneNames.IndexOfByKey(InName);
}

FName FReferenceSkeleton::GetBoneName(int32 BoneIndex) const
{
	return BoneNames.IsValidIndex(BoneIndex) ? BoneNames[BoneIndex] : NAME_None;
}

int32 FReferenceSkeleton::GetParentIndex(int32 BoneIndex) const
{
	return ParentIndices.IsValidIndex(BoneIndex) ? ParentIndices[BoneIndex] : INDEX_NONE;
}

FArchive& operator<<(FArchive& Ar, FReferenceSkeleton& Skeleton)
{
	Ar << Skeleton.BoneNames << Skeleton.ParentIndices << Skeleton.InverseBindPose;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrack& Track)
{
	Ar << Track.Keys;
	return Ar;
}
