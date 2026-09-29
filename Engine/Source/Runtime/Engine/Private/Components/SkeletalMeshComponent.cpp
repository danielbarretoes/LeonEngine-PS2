#include "Components/SkeletalMeshComponent.h"

#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "SkeletalMeshSceneProxy.h"
#include "Stats/Stats.h"

DECLARE_CYCLE_STAT(TEXT("Set Anim Instance"), STAT_SetAnimInstance, STATGROUP_Engine);

DECLARE_CYCLE_STAT(TEXT("Set Skeletal Mesh"), STAT_SetSkeletalMesh, STATGROUP_Engine);

const FAnimUpdateRateSettings& FAnimUpdateRateSettings::Get()
{
	static const FAnimUpdateRateSettings Settings = []()
	{
		FAnimUpdateRateSettings Loaded;
		if (GConfig != nullptr)
		{
			const TCHAR* Section = TEXT("/Script/Engine.AnimationSettings");
			(void)GConfig->GetFloat(Section, TEXT("UpdateRateDistanceStep"), Loaded.UpdateRateDistanceStep, GEngineIni);
			(void)GConfig->GetInt(Section, TEXT("MaxUpdateRate"), Loaded.MaxUpdateRate, GEngineIni);
		}
		Loaded.UpdateRateDistanceStep = FMath::Max(Loaded.UpdateRateDistanceStep, 1.0f);
		Loaded.MaxUpdateRate = FMath::Max(Loaded.MaxUpdateRate, 1);
		return Loaded;
	}();
	return Settings;
}

int32 FAnimUpdateRateSettings::GetUpdateRate(float Distance) const
{
	return FMath::Clamp(1 + FMath::FloorToInt(Distance / UpdateRateDistanceStep), 1, MaxUpdateRate);
}

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
	SCOPE_CYCLE_COUNTER(STAT_SetAnimInstance);
	AnimInstance = Instance != nullptr ? Instance : NewObject<UAnimInstance>(this);
	AnimInstance->SetOwningMeshComponent(this);
	BindAnimInstanceToMesh();
}

void USkeletalMeshComponent::BindAnimInstanceToMesh()
{
	AnimInstance->SetSkeleton(HasValidMesh() ? SkeletalMesh->Skeleton : nullptr);
	bPoseDirty = true;
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	SCOPE_CYCLE_COUNTER(STAT_SetSkeletalMesh);
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

UMaterialInterface* USkeletalMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (HasOverrideMaterial(ElementIndex))
	{
		return OverrideMaterials[ElementIndex];
	}
	return SkeletalMesh != nullptr ? SkeletalMesh->GetMaterial(ElementIndex) : nullptr;
}

int32 USkeletalMeshComponent::GetBoneIndex(FName BoneName) const
{
	return HasValidMesh() && !BoneName.IsNone() ? SkeletalMesh->GetRefSkeleton().FindBoneIndex(BoneName) : INDEX_NONE;
}

void USkeletalMeshComponent::RefreshBoneTransformsIfDirty() const
{
	if (!bPoseDirty)
	{
		return;
	}
	bPoseDirty = false;
	if (!HasValidMesh())
	{
		LocalPose.Reset();
		ComponentSpaceTransforms.Reset();
		return;
	}
	AnimInstance->EvaluatePose(LocalPose);
	FAnimationRuntime::FillUpComponentSpaceTransforms(
		SkeletalMesh->GetRefSkeleton(), LocalPose, ComponentSpaceTransforms);
	++NumPoseEvaluations;
}

void USkeletalMeshComponent::RefreshBoneTransforms()
{
	bPoseDirty = true;
	RefreshBoneTransformsIfDirty();
}

const TArray<FMatrix>& USkeletalMeshComponent::GetComponentSpaceTransforms() const
{
	RefreshBoneTransformsIfDirty();
	return ComponentSpaceTransforms;
}

FBox USkeletalMeshComponent::GetPoseBounds() const
{
	return HasValidMesh() ? SkeletalMesh->GetPoseBounds(GetComponentSpaceTransforms()) : FBox(ForceInit);
}

FTransform USkeletalMeshComponent::GetSocketTransform(FName InSocketName) const
{
	if (InSocketName.IsNone() || !HasValidMesh())
	{
		return GetComponentTransform();
	}
	// A skeleton socket's bone, else a bone of that name; the cached pose's matrix, no sampling here.
	const USkeleton* MeshSkeleton = SkeletalMesh->Skeleton;
	const USkeletalMeshSocket* Socket = MeshSkeleton != nullptr ? MeshSkeleton->FindSocket(InSocketName) : nullptr;
	const int32 BoneIndex = GetBoneIndex(Socket != nullptr ? Socket->BoneName : InSocketName);
	const TArray<FMatrix>& Pose = GetComponentSpaceTransforms();
	if (!Pose.IsValidIndex(BoneIndex))
	{
		return GetComponentTransform();
	}
	const FMatrix BoneToWorld = Pose[BoneIndex] * GetComponentTransform().ToMatrixWithScale();
	return Socket != nullptr ? FTransform(Socket->GetSocketLocalTransform().ToMatrixWithScale() * BoneToWorld)
							 : FTransform(BoneToWorld);
}

bool USkeletalMeshComponent::DoesSocketExist(FName InSocketName) const
{
	if (!HasValidMesh() || InSocketName.IsNone())
	{
		return false;
	}
	const USkeleton* MeshSkeleton = SkeletalMesh->Skeleton;
	return (MeshSkeleton != nullptr && MeshSkeleton->FindSocket(InSocketName) != nullptr) ||
		GetBoneIndex(InSocketName) != INDEX_NONE;
}

bool USkeletalMeshComponent::ShouldRefreshBoneTransforms()
{
	UpdateRate = 1;
	const UWorld* World = GetWorld();
	if (NumPoseEvaluations == 0 || World == nullptr)
	{
		return true;
	}
	if (VisibilityBasedAnimTickOption == EVisibilityBasedAnimTickOption::AlwaysTickPose && !WasRecentlyRendered())
	{
		UpdateRate = 0;
		return false;
	}
	if (!bEnableUpdateRateOptimizations || World->ViewLocationsRenderedLastFrame.Num() == 0)
	{
		return true;
	}
	const FVector Location = GetComponentLocation();
	float NearestSquared = TNumericLimits<float>::Max();
	for (const FVector& ViewLocation : World->ViewLocationsRenderedLastFrame)
	{
		NearestSquared = FMath::Min(NearestSquared, FVector::DistSquared(ViewLocation, Location));
	}
	UpdateRate = FAnimUpdateRateSettings::Get().GetUpdateRate(FMath::Sqrt(NearestSquared));
	// Staggered by the component's id, so that the far meshes spread their evaluations over the frames.
	return ((UpdateCounter + uint32(GetUniqueID())) % uint32(UpdateRate)) == 0;
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
	if (!HasValidMesh())
	{
		return;
	}
	AnimInstance->UpdateAnimation(DeltaTime);
	++UpdateCounter;
	if (ShouldRefreshBoneTransforms())
	{
		RefreshBoneTransforms();
	}
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
	const TArray<FMatrix>& Pose = GetComponentSpaceTransforms();
	// A pose not evaluated again since it was sent (throttled, not drawn) is not sent again.
	if (SentPoseEvaluation == NumPoseEvaluations && SentProxy == SceneProxy)
	{
		return;
	}
	SentPoseEvaluation = NumPoseEvaluations;
	SentProxy = SceneProxy;
	FAnimationRuntime::GetSkinMatrices(SkeletalMesh->GetRefSkeleton(), Pose, SkinMatrices);
	static_cast<FSkeletalMeshSceneProxy*>(SceneProxy)->SetDynamicData(SkinMatrices, SkeletalMesh->GetPoseBounds(Pose));
}
