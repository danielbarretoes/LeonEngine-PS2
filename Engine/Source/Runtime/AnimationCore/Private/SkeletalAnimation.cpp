#include "SkeletalAnimation.h"

FArchive& operator<<(FArchive& Ar, FSkinWeightInfo& Info)
{
	for (int32 Influence = 0; Influence < MaxBoneInfluences; ++Influence)
	{
		Ar << Info.InfluenceBones[Influence];
	}
	for (int32 Influence = 0; Influence < MaxBoneInfluences; ++Influence)
	{
		Ar << Info.InfluenceWeights[Influence];
	}
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

bool FReferenceSkeleton::IsValid() const
{
	const int32 Num = GetNum();
	if (Num > MaxSkinBones || ParentIndices.Num() != Num || RefBonePose.Num() != Num || InverseBindPose.Num() != Num)
	{
		return false;
	}
	for (int32 Bone = 0; Bone < Num; ++Bone)
	{
		const int32 Parent = ParentIndices[Bone];
		if (Parent != INDEX_NONE && (Parent < 0 || Parent >= Bone))
		{
			return false;
		}
		for (int32 Other = 0; Other < Bone; ++Other)
		{
			if (BoneNames[Other] == BoneNames[Bone])
			{
				return false;
			}
		}
	}
	return true;
}

bool FReferenceSkeleton::HasSameBones(const FReferenceSkeleton& Other) const
{
	if (GetNum() != Other.GetNum())
	{
		return false;
	}
	for (int32 Bone = 0; Bone < GetNum(); ++Bone)
	{
		if (GetBoneName(Bone) != Other.GetBoneName(Bone) || GetParentIndex(Bone) != Other.GetParentIndex(Bone))
		{
			return false;
		}
	}
	return true;
}

FArchive& operator<<(FArchive& Ar, FReferenceSkeleton& Skeleton)
{
	Ar << Skeleton.BoneNames << Skeleton.ParentIndices << Skeleton.RefBonePose << Skeleton.InverseBindPose;
	return Ar;
}

FTransform FRawAnimSequenceTrack::GetKey(int32 Frame) const
{
	const FQuat Rotation = RotKeys.Num() > 0 ? RotKeys[FMath::Clamp(Frame, 0, RotKeys.Num() - 1)] : FQuat::Identity;
	const FVector Translation =
		PosKeys.Num() > 0 ? PosKeys[FMath::Clamp(Frame, 0, PosKeys.Num() - 1)] : FVector::ZeroVector;
	const FVector Scale =
		ScaleKeys.Num() > 0 ? ScaleKeys[FMath::Clamp(Frame, 0, ScaleKeys.Num() - 1)] : FVector::OneVector;
	return FTransform(Rotation, Translation, Scale);
}

int32 FRawAnimSequence::GetNumberOfFrames() const
{
	int32 Frames = 0;
	for (const FRawAnimSequenceTrack& Track : Tracks)
	{
		Frames =
			FMath::Max(Frames, FMath::Max(Track.PosKeys.Num(), FMath::Max(Track.RotKeys.Num(), Track.ScaleKeys.Num())));
	}
	return Frames;
}
