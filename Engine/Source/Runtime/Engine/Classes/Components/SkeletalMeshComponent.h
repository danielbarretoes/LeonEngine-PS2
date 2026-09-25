#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "Material.h"
#include "SkeletalAnimation.h"
#include "SkeletalMesh.h"
#include "StaticMesh.h"

class UGameEngine;
class FSceneRenderer;

/** Static mesh glued to a skeletal bone (Unreal-like socket attachment). */
struct ENGINE_API FSkelMeshAttachment
{
	FString BoneName;
	TSharedPtr<UStaticMesh> Mesh;
	FMaterial Material{};
	bool bMaterialOverride = true;
	/** Bone-local TRS applied after the bone model matrix. */
	FTransform Relative;
	/** When true, WorldMatrixOverride replaces component * bone * relative. */
	bool bOverrideWorldMatrix = false;
	FMatrix WorldMatrixOverride = FMatrix::Identity;
};

/** Unreal-like USkeletalMeshComponent — USceneComponent with skeletal mesh + UAnimInstance. */
class ENGINE_API USkeletalMeshComponent : public USceneComponent
{
public:
	USkeletalMeshComponent();

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

	/** Scales the mesh to FitHeight (world units, cm) and stands it on the component origin. */
	void ApplyFitHeight(float FitHeight);

	void ClearAttachments();
	FSkelMeshAttachment& AddAttachment(FSkelMeshAttachment Attachment);
	[[nodiscard]] TArray<FSkelMeshAttachment>& GetAttachments()
	{
		return Attachments;
	}
	[[nodiscard]] const TArray<FSkelMeshAttachment>& GetAttachments() const
	{
		return Attachments;
	}

	/** Bone model-space matrix from the current UAnimInstance pose. */
	[[nodiscard]] bool GetBoneModelMatrix(const FString& InBoneName, FMatrix& OutModel) const;

	/** Attachment relative, then bone, then component world (or WorldMatrixOverride). */
	[[nodiscard]] bool GetAttachmentWorldMatrix(int32 AttachmentIndex, FMatrix& OutWorld) const;

	void TickComponent(float DeltaTime);
	/** Submit using this component's USceneComponent world transform. */
	void SubmitDraw(FSceneRenderer& Renderer) const;

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
	TArray<FSkelMeshAttachment> Attachments;
	mutable TArray<FMatrix> SkinMatrices;
	mutable TArray<FMatrix> BoneWorldMatrices;
};
