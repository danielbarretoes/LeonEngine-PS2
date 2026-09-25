#include "Components/SkeletalMeshComponent.h"

#include "Engine/GameEngine.h"
#include "SceneRenderer.h"
#include "StaticMesh.h"

USkeletalMeshComponent::USkeletalMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Every component starts with the locomotion-only instance (UE creates the anim class's instance at InitAnim).
	AnimInstance = CreateDefaultSubobject<UAnimInstance>(TEXT("AnimInstance"), /*bTransient =*/true);
	AnimInstance->SetOwningMeshComponent(this);
}

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* Instance)
{
	AnimInstance = Instance != nullptr ? Instance : NewObject<UAnimInstance>(this);
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

FTransform USkeletalMeshComponent::GetSocketTransform(FName InSocketName) const
{
	FMatrix BoneModel = FMatrix::Identity;
	if (InSocketName.IsNone() || !GetBoneModelMatrix(InSocketName.ToString(), BoneModel))
	{
		return GetComponentTransform();
	}
	return FTransform(BoneModel * GetComponentTransform().ToMatrixWithScale());
}

bool USkeletalMeshComponent::DoesSocketExist(FName InSocketName) const
{
	return HasValidMesh() && !InSocketName.IsNone() && SkeletalMesh->GetSkeleton().FindBoneIndex(InSocketName) >= 0;
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
}
