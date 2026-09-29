#pragma once

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"
#include "Primitives.h"
#include "SkeletalAnimation.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * The tests' skinned mesh: a 100 cm cube on a two-bone skeleton, "root" at the origin and "top" 50 cm above it (both at
 * rest where they were bound); the corners below the middle follow the root, those above the top. The skeleton has a
 * socket "Tip" 10 cm above the top bone. Needs the mesh builder (the test programs link MeshUtilities).
 */
inline USkeletalMesh* MakeSkinnedTestMesh()
{
	FReferenceSkeleton Bones;
	Bones.BoneNames = {FName(TEXT("root")), FName(TEXT("top"))};
	Bones.ParentIndices = {INDEX_NONE, 0};
	Bones.RefBonePose = {FTransform::Identity, FTransform(FVector(0.0f, 0.0f, 50.0f))};
	Bones.InverseBindPose = {FMatrix::Identity, FTranslationMatrix(FVector(0.0f, 0.0f, -50.0f))};
	USkeleton* Skeleton = NewObject<USkeleton>();
	Skeleton->SetReferenceSkeleton(Bones);
	(void)Skeleton->AddSocket(TEXT("Tip"), TEXT("top"), FTransform(FVector(0.0f, 0.0f, 10.0f)));

	const FMeshData Cube = MakeCube();
	TArray<FSkinWeightInfo> Weights;
	for (const FVertex& Vertex : Cube.Vertices)
	{
		FSkinWeightInfo& Info = Weights.AddDefaulted_GetRef();
		Info.InfluenceBones[0] = Info.InfluenceBones[1] = uint8(Vertex.Position.Z > 0.0f ? 1 : 0);
	}
	USkeletalMesh* Mesh = NewObject<USkeletalMesh>();
	return Mesh->BuildFromMeshData(Cube, Weights, Skeleton) ? Mesh : nullptr;
}

/** A one-frame clip of the test mesh's skeleton: the root moved by RootTranslation, the top at rest. */
inline UAnimSequence* MakeSkinnedTestClip(const FVector& RootTranslation)
{
	FRawAnimSequence Raw;
	Raw.SequenceLength = 1.0f;
	Raw.FrameRate = 30.0f;
	Raw.Tracks.SetNum(2);
	Raw.Tracks[0].PosKeys = {RootTranslation};
	Raw.Tracks[0].RotKeys = {FQuat::Identity};
	Raw.Tracks[1].PosKeys = {FVector(0.0f, 0.0f, 50.0f)};
	Raw.Tracks[1].RotKeys = {FQuat::Identity};
	UAnimSequence* Clip = NewObject<UAnimSequence>();
	(void)Clip->SetFromRawAnimSequence(Raw);
	return Clip;
}

/**
 * The tests' character (Docs/PLANS/ps2-shipping.md N25): a 7-bone skeleton with a lower and an upper body, a 20 cm cube
 * skinned to each bone at rest. Bones (parent, place at rest in the component's space, cm): root (-, 0 0 0), pelvis
 * (root, 0 0 90), thigh_l (pelvis, 0 -10 80), thigh_r (pelvis, 0 10 80), spine_01 (pelvis, 0 0 110), spine_02
 * (spine_01, 0 0 140), hand_r (spine_02, 30 20 140); the socket "Weapon_R" 10 cm along X from hand_r. ExtraBones adds a
 * chain of bones under hand_r (finger_0, finger_1..., each 2 cm along X, without vertices): a character's size for the
 * benchmark.
 */
struct FSkinnedTestCharacter
{
	static constexpr int32 Root = 0;
	static constexpr int32 Pelvis = 1;
	static constexpr int32 ThighL = 2;
	static constexpr int32 ThighR = 3;
	static constexpr int32 Spine01 = 4;
	static constexpr int32 Spine02 = 5;
	static constexpr int32 HandR = 6;
	static constexpr int32 NumBones = 7;

	/** The bones' names and local transforms at rest. */
	static FReferenceSkeleton MakeBones(int32 ExtraBones = 0)
	{
		FReferenceSkeleton Bones;
		Bones.BoneNames = {FName(TEXT("root")), FName(TEXT("pelvis")), FName(TEXT("thigh_l")), FName(TEXT("thigh_r")),
			FName(TEXT("spine_01")), FName(TEXT("spine_02")), FName(TEXT("hand_r"))};
		Bones.ParentIndices = {INDEX_NONE, Root, Pelvis, Pelvis, Pelvis, Spine01, Spine02};
		const FVector Local[NumBones] = {FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 90.0f),
			FVector(0.0f, -10.0f, -10.0f), FVector(0.0f, 10.0f, -10.0f), FVector(0.0f, 0.0f, 20.0f),
			FVector(0.0f, 0.0f, 30.0f), FVector(30.0f, 20.0f, 0.0f)};
		for (int32 Bone = 0; Bone < NumBones; ++Bone)
		{
			Bones.RefBonePose.Add(FTransform(Local[Bone]));
		}
		for (int32 Extra = 0; Extra < ExtraBones; ++Extra)
		{
			Bones.BoneNames.Add(FName(*FString::Printf(TEXT("finger_%d"), Extra)));
			Bones.ParentIndices.Add(Extra == 0 ? HandR : NumBones + Extra - 1);
			Bones.RefBonePose.Add(FTransform(FVector(2.0f, 0.0f, 0.0f)));
		}
		for (int32 Bone = 0; Bone < Bones.BoneNames.Num(); ++Bone)
		{
			Bones.InverseBindPose.Add(FTranslationMatrix(-GetRestLocation(Bones, Bone)));
		}
		return Bones;
	}

	/** A bone's place at rest in the component's space. */
	static FVector GetRestLocation(const FReferenceSkeleton& Bones, int32 Bone)
	{
		FVector Location = FVector::ZeroVector;
		for (int32 Index = Bone; Index != INDEX_NONE; Index = Bones.ParentIndices[Index])
		{
			Location += Bones.RefBonePose[Index].GetTranslation();
		}
		return Location;
	}

	/** The skeleton asset (with the Weapon_R socket). */
	static USkeleton* MakeSkeleton(int32 ExtraBones = 0)
	{
		USkeleton* Skeleton = NewObject<USkeleton>();
		Skeleton->SetReferenceSkeleton(MakeBones(ExtraBones));
		(void)Skeleton->AddSocket(TEXT("Weapon_R"), TEXT("hand_r"), FTransform(FVector(10.0f, 0.0f, 0.0f)));
		return Skeleton;
	}

	/** The mesh: a 20 cm cube at each bone, skinned to it alone. Null when the mesh builder is missing. */
	static USkeletalMesh* MakeMesh(USkeleton* Skeleton = nullptr, int32 ExtraBones = 0)
	{
		USkeleton* LocalSkeleton = Skeleton != nullptr ? Skeleton : MakeSkeleton(ExtraBones);
		const FReferenceSkeleton& Bones = LocalSkeleton->GetReferenceSkeleton();
		const FMeshData Cube = MakeCube();
		FMeshData Data = Cube;
		Data.Vertices.Reset();
		Data.Indices.Reset();
		TArray<FSkinWeightInfo> Weights;
		for (int32 Bone = 0; Bone < NumBones; ++Bone)
		{
			const uint32 Base = uint32(Data.Vertices.Num());
			const FVector Center = GetRestLocation(Bones, Bone);
			for (const FVertex& Vertex : Cube.Vertices)
			{
				FVertex& Moved = Data.Vertices.Add_GetRef(Vertex);
				Moved.Position = Center + (Vertex.Position * 0.2f);
				FSkinWeightInfo& Info = Weights.AddDefaulted_GetRef();
				Info.InfluenceBones[0] = Info.InfluenceBones[1] = uint8(Bone);
			}
			for (const uint32 Index : Cube.Indices)
			{
				Data.Indices.Add(Base + Index);
			}
		}
		Data.Submeshes.Reset();
		Data.Submeshes.Add(FMeshSection{0, Data.Indices.Num(), 0});
		USkeletalMesh* Mesh = NewObject<USkeletalMesh>();
		return Mesh->BuildFromMeshData(Data, Weights, LocalSkeleton) ? Mesh : nullptr;
	}

	/**
	 * A clip of the character: NumFrames frames at 30 Hz, each the rest pose changed by MakeFrame(Frame, Pose) (Pose is
	 * the local transforms, one per bone). Looping unless bInLoop is false.
	 */
	template <typename FrameFunctionType>
	static UAnimSequence* MakeClip(
		int32 NumFrames, FrameFunctionType&& MakeFrame, bool bInLoop = true, int32 ExtraBones = 0)
	{
		const FReferenceSkeleton Bones = MakeBones(ExtraBones);
		FRawAnimSequence Raw;
		Raw.FrameRate = 30.0f;
		Raw.SequenceLength = float(FMath::Max(NumFrames - 1, 1)) / Raw.FrameRate;
		Raw.bLoop = bInLoop;
		Raw.Tracks.SetNum(Bones.GetNum());
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			TArray<FTransform> Pose = Bones.RefBonePose;
			MakeFrame(Frame, Pose);
			for (int32 Bone = 0; Bone < Bones.GetNum(); ++Bone)
			{
				Raw.Tracks[Bone].PosKeys.Add(Pose[Bone].GetTranslation());
				Raw.Tracks[Bone].RotKeys.Add(Pose[Bone].GetRotation());
			}
		}
		UAnimSequence* Clip = NewObject<UAnimSequence>();
		(void)Clip->SetFromRawAnimSequence(Raw);
		return Clip;
	}

	/** A one-frame clip: the rest pose with Bone moved by Offset (cm, in its parent's space). */
	static UAnimSequence* MakeOffsetClip(int32 Bone, const FVector& Offset, float Length = 1.0f)
	{
		UAnimSequence* Clip = MakeClip(2,
			[&](int32, TArray<FTransform>& Pose) { Pose[Bone].SetTranslation(Pose[Bone].GetTranslation() + Offset); });
		Clip->SequenceLength = Length;
		return Clip;
	}

	/** A one-frame clip: the rest pose with Bone turned by Rotation. */
	static UAnimSequence* MakeRotatedClip(int32 Bone, const FQuat& Rotation, int32 ExtraBones = 0)
	{
		return MakeClip(
			2, [&](int32, TArray<FTransform>& Pose) { Pose[Bone].SetRotation(Rotation); }, true, ExtraBones);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
