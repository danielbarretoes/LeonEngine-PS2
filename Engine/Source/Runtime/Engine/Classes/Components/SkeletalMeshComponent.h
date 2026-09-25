#pragma once

#include "Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "Material.h"
#include "SkeletalAnimation.h"
#include "SkeletalMesh.h"
#include "SkeletalMeshComponent.generated.h"

class UGameEngine;
class FSceneRenderer;

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

	void SetAnimInstance(TUniquePtr<UAnimInstance> Instance);
	template <typename TAnim, typename... ArgsType>
	TAnim& SetAnimInstance(ArgsType&&... Args)
	{
		static_assert(TIsDerivedFrom<TAnim, UAnimInstance>::Value, "TAnim must derive from AnimInstance");
		auto Owned = MakeUnique<TAnim>(Forward<ArgsType>(Args)...);
		TAnim& Ref = *Owned;
		SetAnimInstance(MoveTemp(Owned));
		return Ref;
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
		return dynamic_cast<TAnim*>(AnimInstance.Get());
	}
	template <typename TAnim>
	[[nodiscard]] const TAnim* GetAnimInstance() const
	{
		return dynamic_cast<const TAnim*>(AnimInstance.Get());
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
	/** Submit using this component's USceneComponent world transform. */
	void SubmitDraw(FSceneRenderer& Renderer) const override;

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
	TUniquePtr<UAnimInstance> AnimInstance;
	mutable TArray<FMatrix> SkinMatrices;
	mutable TArray<FMatrix> BoneWorldMatrices;
};
