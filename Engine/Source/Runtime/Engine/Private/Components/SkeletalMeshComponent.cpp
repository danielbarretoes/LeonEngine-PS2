#include "Components/SkeletalMeshComponent.h"

#include "Engine/GameEngine.h"
#include "SceneRenderer.h"
#include "StaticMesh.h"

USkeletalMeshComponent::USkeletalMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AnimInstance(MakeUnique<UAnimInstance>())
{
	AnimInstance->SetOwningMeshComponent(this);
}

void USkeletalMeshComponent::SetAnimInstance(TUniquePtr<UAnimInstance> Instance)
{
	AnimInstance = MoveTemp(Instance);
	if (AnimInstance == nullptr)
	{
		AnimInstance = MakeUnique<UAnimInstance>();
	}
	AnimInstance->SetOwningMeshComponent(this);
	BindAnimInstanceToAssets();
}

void USkeletalMeshComponent::BindAnimInstanceToAssets()
{
	if (SkeletalMesh != nullptr && SkeletalMesh->Valid())
	{
		AnimInstance->SetSkeleton(&SkeletalMesh->GetSkeleton());
	}
	else
	{
		AnimInstance->SetSkeleton(nullptr);
	}
	if (BlendSpace.Samples.Num() > 0)
	{
		AnimInstance->SetBlendSpace(&BlendSpace);
	}
}

UAnimSequence* USkeletalMeshComponent::FindSequence(const FString& Name)
{
	const int32* Index = SequenceIndexByName.Find(Name);
	return Index != nullptr ? Sequences[*Index].Get() : nullptr;
}

const UAnimSequence* USkeletalMeshComponent::FindSequence(const FString& Name) const
{
	const int32* Index = SequenceIndexByName.Find(Name);
	return Index != nullptr ? Sequences[*Index].Get() : nullptr;
}

UAnimSequence& USkeletalMeshComponent::GetOrCreateSequence(const FString& Name)
{
	if (const int32* Index = SequenceIndexByName.Find(Name))
	{
		return *Sequences[*Index];
	}
	UAnimSequence& Sequence = *Sequences.Add_GetRef(MakeUnique<UAnimSequence>());
	Sequence.Name = FName(*Name);
	SequenceIndexByName.Add(Name, Sequences.Num() - 1);
	return Sequence;
}

void USkeletalMeshComponent::BindSequencesToAnimInstance()
{
	BindAnimInstanceToAssets();
	AnimInstance->NativeInitializeAnimation();
}

void USkeletalMeshComponent::SetSkeletalMesh(TSharedPtr<USkeletalMesh> InMesh)
{
	SkeletalMesh = MoveTemp(InMesh);
	if (SkeletalMesh != nullptr && SkeletalMesh->Valid())
	{
		AnimInstance->SetSkeleton(&SkeletalMesh->GetSkeleton());
	}
	else
	{
		AnimInstance->SetSkeleton(nullptr);
	}
}

void USkeletalMeshComponent::ApplyFitHeight(float FitHeight)
{
	if (!HasValidMesh() || FitHeight <= 0.0f)
	{
		return;
	}
	const float Scale = SkeletalMesh->FitUniformScale(FitHeight);
	/** cm above the floor, against z-fighting with the ground. */
	constexpr float GroundEpsilon = 0.8f;
	const FVector Mn = SkeletalMesh->GetLocalMin();
	const FVector Mx = SkeletalMesh->GetLocalMax();
	const FVector Center = (Mn + Mx) * 0.5f;
	RelativeScale3D = FVector(Scale, Scale, Scale);
	// Centred on the component origin in X / Y and standing on it; the offset is in the mesh's space, so it turns with
	// the relative rotation.
	const FVector Grounded((-Center.X) * Scale, (-Center.Y) * Scale, (-Mn.Z) * Scale);
	RelativeLocation = RelativeRotation.RotateVector(Grounded) + FVector(0.0f, 0.0f, GroundEpsilon);
}

void USkeletalMeshComponent::ClearAttachments()
{
	Attachments.Empty();
}

FSkelMeshAttachment& USkeletalMeshComponent::AddAttachment(FSkelMeshAttachment Attachment)
{
	Attachments.Add(MoveTemp(Attachment));
	return Attachments.Last();
}

bool USkeletalMeshComponent::GetBoneModelMatrix(const FString& InBoneName, FMatrix& OutModel) const
{
	if (!HasValidMesh() || InBoneName.IsEmpty())
	{
		return false;
	}
	const int32 BoneIndex = SkeletalMesh->GetSkeleton().FindBoneIndex(FName(*InBoneName));
	if (BoneIndex < 0)
	{
		return false;
	}
	AnimInstance->GetBoneWorldMatrices(BoneWorldMatrices);
	if (BoneWorldMatrices.Num() <= BoneIndex)
	{
		return false;
	}
	OutModel = BoneWorldMatrices[BoneIndex];
	return true;
}

bool USkeletalMeshComponent::GetAttachmentWorldMatrix(int32 AttachmentIndex, FMatrix& OutWorld) const
{
	if (!Attachments.IsValidIndex(AttachmentIndex))
	{
		return false;
	}
	const FSkelMeshAttachment& Att = Attachments[AttachmentIndex];
	if (Att.bOverrideWorldMatrix)
	{
		OutWorld = Att.WorldMatrixOverride;
		return true;
	}
	FMatrix BoneModel = FMatrix::Identity;
	if (!GetBoneModelMatrix(Att.BoneName, BoneModel))
	{
		return false;
	}
	OutWorld = Att.Relative.ToMatrixWithScale() * BoneModel * GetComponentTransform().ToMatrixWithScale();
	return true;
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
	if (!HasValidMesh())
	{
		return;
	}
	AnimInstance->NativeUpdateAnimation(DeltaTime);
}

void USkeletalMeshComponent::SubmitDraw(FSceneRenderer& Renderer) const
{
	if (!HasValidMesh())
	{
		return;
	}

	AnimInstance->GetSkinMatrices(SkinMatrices);
	Renderer.SubmitSkeletalDraw(*SkeletalMesh, GetComponentTransform(), SkinMatrices);

	for (int32 I = 0; I < Attachments.Num(); ++I)
	{
		const FSkelMeshAttachment& Att = Attachments[I];
		if (Att.Mesh == nullptr || !Att.Mesh->Valid())
		{
			continue;
		}
		FMatrix AttachmentWorld = FMatrix::Identity;
		if (!GetAttachmentWorldMatrix(I, AttachmentWorld))
		{
			continue;
		}
		Renderer.SubmitStaticDraw(*Att.Mesh, AttachmentWorld, Att.Material);
	}
}
