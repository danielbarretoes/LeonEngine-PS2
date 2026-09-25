#include "Components/SkeletalMeshComponent.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "SkeletalMeshSceneProxy.h"

USkeletalMeshComponent::USkeletalMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Every component starts with the locomotion-only instance (UE creates the anim class's instance at InitAnim).
	AnimInstance = CreateDefaultSubobject<UAnimInstance>(TEXT("AnimInstance"), /*bTransient =*/true);
	AnimInstance->SetOwningMeshComponent(this);
}

bool USkeletalMeshComponent::HasValidMesh() const
{
	return SkeletalMesh != nullptr && SkeletalMesh->HasValidRenderData();
}

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* Instance)
{
	AnimInstance = Instance != nullptr ? Instance : NewObject<UAnimInstance>(this);
	AnimInstance->SetOwningMeshComponent(this);
	BindAnimInstanceToMesh();
}

void USkeletalMeshComponent::BindAnimInstanceToMesh()
{
	AnimInstance->SetSkeleton(HasValidMesh() ? SkeletalMesh->Skeleton : nullptr);
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	SkeletalMesh = InMesh;
	BindAnimInstanceToMesh();
	MarkRenderStateDirty();
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
	const FVector Mn = SkeletalMesh->GetBoundingBox().Min;
	const FVector Mx = SkeletalMesh->GetBoundingBox().Max;
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
	const int32 BoneIndex = SkeletalMesh->GetRefSkeleton().FindBoneIndex(FName(*InBoneName));
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
	if (InSocketName.IsNone())
	{
		return GetComponentTransform();
	}
	FMatrix BoneModel = FMatrix::Identity;
	const USkeleton* MeshSkeleton = HasValidMesh() ? SkeletalMesh->Skeleton : nullptr;
	if (const USkeletalMeshSocket* Socket = MeshSkeleton != nullptr ? MeshSkeleton->FindSocket(InSocketName) : nullptr)
	{
		if (GetBoneModelMatrix(Socket->BoneName.ToString(), BoneModel))
		{
			return FTransform(Socket->GetSocketLocalTransform().ToMatrixWithScale() * BoneModel *
				GetComponentTransform().ToMatrixWithScale());
		}
		return GetComponentTransform();
	}
	if (!GetBoneModelMatrix(InSocketName.ToString(), BoneModel))
	{
		return GetComponentTransform();
	}
	return FTransform(BoneModel * GetComponentTransform().ToMatrixWithScale());
}

bool USkeletalMeshComponent::DoesSocketExist(FName InSocketName) const
{
	if (!HasValidMesh() || InSocketName.IsNone())
	{
		return false;
	}
	const USkeleton* MeshSkeleton = SkeletalMesh->Skeleton;
	return (MeshSkeleton != nullptr && MeshSkeleton->FindSocket(InSocketName) != nullptr) ||
		SkeletalMesh->GetRefSkeleton().FindBoneIndex(InSocketName) >= 0;
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
	if (!HasValidMesh())
	{
		return;
	}
	AnimInstance->NativeUpdateAnimation(DeltaTime);
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
	return HasValidMesh() ? new FSkeletalMeshSceneProxy(this) : nullptr;
}

void USkeletalMeshComponent::SendRenderDynamicData_Concurrent()
{
	if (SceneProxy == nullptr || !HasValidMesh())
	{
		return;
	}
	AnimInstance->GetSkinMatrices(SkinMatrices);
	static_cast<FSkeletalMeshSceneProxy*>(SceneProxy)->SetBoneMatrices(SkinMatrices);
}
