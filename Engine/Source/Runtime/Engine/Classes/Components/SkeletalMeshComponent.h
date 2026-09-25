#pragma once

#include "Components/SceneComponent.h"
#include "Material.h"
#include "Migration/LegacyTransform.h"
#include "SkeletalAnimation.h"
#include "SkeletalMesh.h"
#include "StaticMesh.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <deque>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

class UGameEngine;
class FSceneRenderer;

/// Static mesh glued to a skeletal bone (Unreal-like socket attachment).
struct ENGINE_API FSkelMeshAttachment
{
	std::string BoneName;
	std::shared_ptr<UStaticMesh> Mesh;
	FMaterial Material{};
	bool bMaterialOverride = true;
	/// Bone-local TRS applied after the bone model matrix.
	FLegacyTransform Relative{};
	/// When true, `worldMatrixOverride` replaces `component * bone * relative`.
	bool bOverrideWorldMatrix = false;
	glm::mat4 WorldMatrixOverride{1.0f};
};

/// Unreal-like USkeletalMeshComponent — USceneComponent with skeletal mesh + UAnimInstance.
class ENGINE_API USkeletalMeshComponent : public USceneComponent
{
public:
	USkeletalMeshComponent();

	void SetSkeletalMesh(std::shared_ptr<USkeletalMesh> InMesh);
	[[nodiscard]] USkeletalMesh* GetSkeletalMesh()
	{
		return SkeletalMesh.get();
	}
	[[nodiscard]] const USkeletalMesh* GetSkeletalMesh() const
	{
		return SkeletalMesh.get();
	}

	void SetAnimInstance(std::unique_ptr<UAnimInstance> Instance);
	template <typename TAnim, typename... ArgsType>
	TAnim& SetAnimInstance(ArgsType&&... Args)
	{
		static_assert(std::is_base_of_v<UAnimInstance, TAnim>, "TAnim must derive from AnimInstance");
		auto Owned = std::make_unique<TAnim>(std::forward<ArgsType>(Args)...);
		TAnim& Ref = *Owned;
		SetAnimInstance(std::move(Owned));
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
		return dynamic_cast<TAnim*>(AnimInstance.get());
	}
	template <typename TAnim>
	[[nodiscard]] const TAnim* GetAnimInstance() const
	{
		return dynamic_cast<const TAnim*>(AnimInstance.get());
	}

	[[nodiscard]] UBlendSpace1D& GetBlendSpace()
	{
		return BlendSpace;
	}
	[[nodiscard]] const UBlendSpace1D& GetBlendSpace() const
	{
		return BlendSpace;
	}

	[[nodiscard]] UAnimSequence* FindSequence(const std::string& Name);
	[[nodiscard]] const UAnimSequence* FindSequence(const std::string& Name) const;
	[[nodiscard]] UAnimSequence& GetOrCreateSequence(const std::string& Name);

	/// Bind skeleton/blendspace pointers and call UAnimInstance::NativeInitializeAnimation.
	void BindSequencesToAnimInstance();

	void ApplyFitHeight(float FitHeight);

	void ClearAttachments();
	FSkelMeshAttachment& AddAttachment(FSkelMeshAttachment Attachment);
	[[nodiscard]] std::vector<FSkelMeshAttachment>& GetAttachments()
	{
		return Attachments;
	}
	[[nodiscard]] const std::vector<FSkelMeshAttachment>& GetAttachments() const
	{
		return Attachments;
	}

	/// Bone model-space matrix from the current UAnimInstance pose.
	[[nodiscard]] bool GetBoneModelMatrix(const std::string& InBoneName, glm::mat4& OutModel) const;

	/// Component world * bone * attachment.relative (or worldMatrixOverride).
	[[nodiscard]] bool GetAttachmentWorldMatrix(std::size_t AttachmentIndex, glm::mat4& OutWorld) const;

	void TickComponent(float DeltaTime);
	/// Submit using this component's USceneComponent world transform.
	void SubmitDraw(FSceneRenderer& Renderer) const;

	[[nodiscard]] bool HasValidMesh() const
	{
		return SkeletalMesh != nullptr && SkeletalMesh->Valid();
	}

private:
	void BindAnimInstanceToAssets();

	std::shared_ptr<USkeletalMesh> SkeletalMesh;
	/// Stable storage — BlendSpace / UAnimInstance keep raw pointers into these elements.
	std::deque<UAnimSequence> Sequences;
	std::unordered_map<std::string, std::size_t> SequenceIndexByName;
	UBlendSpace1D BlendSpace{};
	std::unique_ptr<UAnimInstance> AnimInstance;
	std::vector<FSkelMeshAttachment> Attachments;
	mutable TArray<FMatrix> SkinMatrices;
	mutable TArray<FMatrix> BoneWorldMatrices;
};
