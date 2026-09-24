#include "Components/SkeletalMeshComponent.h"

#include "Engine/GameEngine.h"
#include "SceneRenderer.h"
#include "StaticMesh.h"

#include <glm/common.hpp>
#include <glm/vec3.hpp>

#include <iostream>

USkeletalMeshComponent::USkeletalMeshComponent()
	: AnimInstance(std::make_unique<UAnimInstance>())
{
	AnimInstance->SetOwningMeshComponent(this);
}

void USkeletalMeshComponent::SetAnimInstance(std::unique_ptr<UAnimInstance> Instance)
{
	AnimInstance = std::move(Instance);
	if (AnimInstance == nullptr)
	{
		AnimInstance = std::make_unique<UAnimInstance>();
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
	if (!BlendSpace.Samples.empty())
	{
		AnimInstance->SetBlendSpace(&BlendSpace);
	}
}

UAnimSequence* USkeletalMeshComponent::FindSequence(const std::string& Name)
{
	const auto It = SequenceIndexByName.find(Name);
	return It != SequenceIndexByName.end() ? &Sequences[It->second] : nullptr;
}

const UAnimSequence* USkeletalMeshComponent::FindSequence(const std::string& Name) const
{
	const auto It = SequenceIndexByName.find(Name);
	return It != SequenceIndexByName.end() ? &Sequences[It->second] : nullptr;
}

UAnimSequence& USkeletalMeshComponent::GetOrCreateSequence(const std::string& Name)
{
	if (const auto It = SequenceIndexByName.find(Name); It != SequenceIndexByName.end())
	{
		return Sequences[It->second];
	}
	Sequences.push_back(UAnimSequence{});
	Sequences.back().Name = Name;
	SequenceIndexByName[Name] = Sequences.size() - 1;
	return Sequences.back();
}

void USkeletalMeshComponent::BindSequencesToAnimInstance()
{
	BindAnimInstanceToAssets();
	AnimInstance->NativeInitializeAnimation();
}

void USkeletalMeshComponent::SetSkeletalMesh(std::shared_ptr<USkeletalMesh> InMesh)
{
	SkeletalMesh = std::move(InMesh);
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
	constexpr float GroundEpsilon = 0.008f;
	const glm::vec3 Mn = SkeletalMesh->GetLocalMin();
	const glm::vec3 Mx = SkeletalMesh->GetLocalMax();
	const glm::vec3 Center = (Mn + Mx) * 0.5f;
	RelativeScale = {Scale, Scale, Scale};
	RelativeLocation = {(-Center.x) * Scale, ((-Mn.y) * Scale) + GroundEpsilon, (-Center.z) * Scale};
}

void USkeletalMeshComponent::ClearAttachments()
{
	Attachments.clear();
}

FSkelMeshAttachment& USkeletalMeshComponent::AddAttachment(FSkelMeshAttachment Attachment)
{
	Attachments.push_back(std::move(Attachment));
	return Attachments.back();
}

bool USkeletalMeshComponent::GetBoneModelMatrix(const std::string& InBoneName, glm::mat4& OutModel) const
{
	if (!HasValidMesh() || InBoneName.empty())
	{
		return false;
	}
	const int BoneIndex = SkeletalMesh->GetSkeleton().FindBoneIndex(InBoneName);
	if (BoneIndex < 0)
	{
		return false;
	}
	AnimInstance->GetBoneWorldMatrices(BoneWorldMatrices);
	if (BoneWorldMatrices.size() <= static_cast<std::size_t>(BoneIndex))
	{
		return false;
	}
	OutModel = BoneWorldMatrices[static_cast<std::size_t>(BoneIndex)];
	return true;
}

bool USkeletalMeshComponent::GetAttachmentWorldMatrix(std::size_t AttachmentIndex, glm::mat4& OutWorld) const
{
	if (AttachmentIndex >= Attachments.size())
	{
		return false;
	}
	const FSkelMeshAttachment& Att = Attachments[AttachmentIndex];
	if (Att.bOverrideWorldMatrix)
	{
		OutWorld = Att.WorldMatrixOverride;
		return true;
	}
	glm::mat4 BoneModel{};
	if (!GetBoneModelMatrix(Att.BoneName, BoneModel))
	{
		return false;
	}
	OutWorld = GetComponentTransform() * BoneModel * Att.Relative.ModelMatrix();
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

	for (std::size_t I = 0; I < Attachments.size(); ++I)
	{
		const FSkelMeshAttachment& Att = Attachments[I];
		if (Att.Mesh == nullptr || !Att.Mesh->Valid())
		{
			continue;
		}
		glm::mat4 AttachmentWorld{};
		if (!GetAttachmentWorldMatrix(I, AttachmentWorld))
		{
			continue;
		}
		Renderer.SubmitStaticDraw(*Att.Mesh, AttachmentWorld, Att.Material);
	}
}
