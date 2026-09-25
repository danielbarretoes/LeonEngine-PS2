#pragma once

#include "Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "Material.h"
#include "SkeletalAnimation.h"
#include "SkeletalMesh.h"
#include "SkeletalMeshComponent.generated.h"

class UGameEngine;

/**
 * Unreal-like USkeletalMeshComponent — a mesh component with a skeletal mesh + UAnimInstance (UE derives it from
 * USkinnedMeshComponent; Leon has no skinned base yet).
 *
 * Its bones are sockets: a component attached at a bone name (AttachToComponent / SetupAttachment with the socket
 * name, a UStaticMeshComponent weapon for example) follows the animated bone.
 */
UCLASS()
class ENGINE_API USkeletalMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	USkeletalMeshComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetSkeletalMesh(TSharedPtr<USkeletalMesh> InMesh);
	[[nodiscard]] USkeletalMesh* GetSkeletalMesh()
	{
		return SkeletalMesh.Get();
	}
	[[nodiscard]] const USkeletalMesh* GetSkeletalMesh() const
	{
		return SkeletalMesh.Get();
	}
	/** The mesh with its shared ownership (the renderer's proxy keeps it alive). */
	[[nodiscard]] const TSharedPtr<USkeletalMesh>& GetSkeletalMeshShared() const
	{
		return SkeletalMesh;
	}

	/** Replaces the anim instance (a new UAnimInstance when null); the component becomes its owner. */
	void SetAnimInstance(UAnimInstance* Instance);
	/** Creates a TAnim with this component as outer (UE: the anim class's instance) and uses it. */
	template <typename TAnim>
	TAnim& SetAnimInstance()
	{
		static_assert(TIsDerivedFrom<TAnim, UAnimInstance>::Value, "TAnim must derive from AnimInstance");
		TAnim* NewInstance = NewObject<TAnim>(this);
		SetAnimInstance(NewInstance);
		return *NewInstance;
	}

	[[nodiscard]] UAnimInstance& GetAnimInstance()
	{
		return *AnimInstance;
	}
	[[nodiscard]] const UAnimInstance& GetAnimInstance() const
	{
		return *AnimInstance;
	}

	template <typename TAnim>
	[[nodiscard]] TAnim* GetAnimInstance()
	{
		return Cast<TAnim>(AnimInstance);
	}
	template <typename TAnim>
	[[nodiscard]] const TAnim* GetAnimInstance() const
	{
		return Cast<TAnim>(AnimInstance);
	}

	[[nodiscard]] UBlendSpace1D& GetBlendSpace()
	{
		return BlendSpace;
	}
	[[nodiscard]] const UBlendSpace1D& GetBlendSpace() const
	{
		return BlendSpace;
	}

	[[nodiscard]] UAnimSequence* FindSequence(const FString& Name);
	[[nodiscard]] const UAnimSequence* FindSequence(const FString& Name) const;
	[[nodiscard]] UAnimSequence& GetOrCreateSequence(const FString& Name);

	/** Bind skeleton/blendspace pointers and call UAnimInstance::NativeInitializeAnimation. */
	void BindSequencesToAnimInstance();

	/** Scales the mesh to FitHeight (world units, cm; its Z extent) and stands it on the component origin. */
	void ApplyFitHeight(float FitHeight);

	/** Bone model-space matrix from the current UAnimInstance pose. */
	[[nodiscard]] bool GetBoneModelMatrix(const FString& InBoneName, FMatrix& OutModel) const;

	/** A bone's world transform (the bone's pose, then the component transform); the component's for other names. */
	[[nodiscard]] FTransform GetSocketTransform(FName InSocketName) const override;
	/** True for the names of the mesh's bones. */
	[[nodiscard]] bool DoesSocketExist(FName InSocketName) const override;

	void TickComponent(float DeltaTime) override;
	/** A FSkeletalMeshSceneProxy for a valid mesh (UE: CreateSceneProxy). */
	[[nodiscard]] FPrimitiveSceneProxy* CreateSceneProxy() override;
	/** Sends the pose's skin matrices to the proxy (UE: SendRenderDynamicData_Concurrent). */
	void SendRenderDynamicData_Concurrent() override;

	[[nodiscard]] bool HasValidMesh() const
	{
		return SkeletalMesh != nullptr && SkeletalMesh->Valid();
	}

private:
	void BindAnimInstanceToAssets();

	TSharedPtr<USkeletalMesh> SkeletalMesh;
	/** Heap elements: BlendSpace / UAnimInstance keep raw pointers to the sequences. */
	TArray<TUniquePtr<UAnimSequence>> Sequences;
	TMap<FString, int32> SequenceIndexByName;
	UBlendSpace1D BlendSpace{};
	/** The animation instance, an inner object of the component (UE: AnimScriptInstance). */
	UPROPERTY(Transient)
	UAnimInstance* AnimInstance = nullptr;
	mutable TArray<FMatrix> SkinMatrices;
	mutable TArray<FMatrix> BoneWorldMatrices;
};
