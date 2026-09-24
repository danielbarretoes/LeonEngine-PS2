#include "Components/SkeletalMeshComponent.h"

#include "Animation/CookedSkeletal.h"
#include "Engine/GameEngine.h"
#include "Misc/Paths.h"
#include "SceneRenderer.h"
#include "StaticMesh.h"

#include <glm/common.hpp>
#include <glm/vec3.hpp>

#include <filesystem>
#include <iostream>

namespace
{

	namespace fs = std::filesystem;

	[[nodiscard]] std::string JoinRel(const std::string& BaseDir, const std::string& Rel)
	{
		return (fs::path(BaseDir) / Rel).lexically_normal().string();
	}

	[[nodiscard]] std::string SequenceKeyFromAnimRel(const std::string& AnimRel)
	{
		std::string Key = fs::path(AnimRel).filename().string();
		const auto Dot = Key.find('.');
		if (Dot != std::string::npos)
		{
			Key.resize(Dot);
		}
		return Key;
	}

} // namespace

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

bool USkeletalMeshComponent::LoadFromFbx(const std::string& MeshFbxPath, const std::string& RunFbxPath, float FitHeight)
{
	FSkeletalMeshData Data;
	const std::string MeshPath = FPaths::ResolveAssetPath(MeshFbxPath);
	if (!LoadSkeletalMeshFromFbx(MeshPath, Data))
	{
		std::cerr << "SkeletalMeshComponent: failed to load '" << MeshPath << "'\n";
		return false;
	}

	Sequences.clear();
	SequenceIndexByName.clear();
	UAnimSequence& Idle = GetOrCreateSequence("BreathingIdle");
	Idle = std::move(Data.EmbeddedAnim);
	Idle.Name = "BreathingIdle";
	if (Idle.FrameCount() <= 0)
	{
		std::cerr << "SkeletalMeshComponent: mesh FBX has no embedded AnimSequence\n";
	}

	auto LocalMesh = std::make_shared<USkeletalMesh>(USkeletalMesh::Upload(std::move(Data)));
	if (LocalMesh == nullptr || !LocalMesh->Valid())
	{
		std::cerr << "SkeletalMeshComponent: GPU upload failed\n";
		return false;
	}
	LocalMesh->GetMaterial().Albedo = {0.72f, 0.74f, 0.78f};
	LocalMesh->GetMaterial().Shininess = 24.0f;
	LocalMesh->GetMaterial().SyncRoughnessFromShininess();

	UAnimSequence& Run = GetOrCreateSequence("Running");
	const std::string RunPath = FPaths::ResolveAssetPath(RunFbxPath);
	if (!LoadAnimSequenceFromFbx(RunPath, LocalMesh->GetSkeleton(), Run))
	{
		std::cerr << "SkeletalMeshComponent: failed to load run AnimSequence '" << RunPath << "'\n";
	}
	Run.Name = "Running";

	SetSkeletalMesh(std::move(LocalMesh));
	ApplyFitHeight(FitHeight);
	BindSequencesToAnimInstance();

	std::cout << "SkeletalMeshComponent: loaded FBX (" << SkeletalMesh->GetSkeleton().BoneCount() << " bones)\n";
	return true;
}

bool USkeletalMeshComponent::LoadFromCooked(UGameEngine& Engine, const std::string& CharacterAssetPath)
{
	const std::string CharacterPath = FPaths::ResolveAssetPath(CharacterAssetPath);
	FCharacterVisualDesc Desc;
	if (!LoadCharacterVisual(CharacterPath, Desc))
	{
		std::cerr << "SkeletalMeshComponent: failed to load character '" << CharacterPath << "'\n";
		return false;
	}

	const std::string BaseDir = fs::path(CharacterPath).parent_path().string();
	const std::string MeshJson = JoinRel(BaseDir, Desc.SkeletalMeshRel);
	const std::string BlendJson = JoinRel(BaseDir, Desc.BlendSpaceRel);

	FSkeletalMeshData MeshData;
	std::string MaterialRel;
	if (!LoadSkeletalMesh(MeshJson, MeshData, nullptr, &MaterialRel))
	{
		std::cerr << "SkeletalMeshComponent: failed cooked skelmesh '" << MeshJson << "'\n";
		return false;
	}

	auto LocalMesh = std::make_shared<USkeletalMesh>(Engine.GetResources().IsGpuUploadEnabled()
			? USkeletalMesh::Upload(std::move(MeshData))
			: USkeletalMesh::CreateCpu(std::move(MeshData)));
	if (LocalMesh == nullptr || !LocalMesh->Valid())
	{
		std::cerr << "SkeletalMeshComponent: cooked mesh create failed\n";
		return false;
	}
	if (!MaterialRel.empty())
	{
		const std::string MatPath = JoinRel(fs::path(MeshJson).parent_path().string(), MaterialRel);
		LocalMesh->SetMaterial(Engine.GetResources().LoadMaterial(MatPath));
	}
	else
	{
		LocalMesh->GetMaterial().Albedo = {0.72f, 0.74f, 0.78f};
		LocalMesh->GetMaterial().Shininess = 24.0f;
		LocalMesh->GetMaterial().SyncRoughnessFromShininess();
	}
	SetSkeletalMesh(std::move(LocalMesh));

	FBlendSpace1DAssetDesc BsDesc;
	if (!LoadBlendSpace1DJson(BlendJson, BsDesc) || BsDesc.Samples.empty())
	{
		std::cerr << "SkeletalMeshComponent: failed blendspace '" << BlendJson << "'\n";
		return false;
	}

	Sequences.clear();
	SequenceIndexByName.clear();
	const std::string BlendDir = fs::path(BlendJson).parent_path().string();
	BlendSpace.Name = BsDesc.Name;
	BlendSpace.AxisMin = BsDesc.AxisMin;
	BlendSpace.AxisMax = BsDesc.AxisMax;
	BlendSpace.ClearSamples();

	// Optional jump clips first; deque keeps pointers stable across later inserts.
	auto LoadNamed = [&](const std::string& Rel, const char* FallbackKey)
	{
		if (Rel.empty())
		{
			return;
		}
		const std::string Key = SequenceKeyFromAnimRel(Rel);
		const std::string Name = Key.empty() ? FallbackKey : Key;
		UAnimSequence& Seq = GetOrCreateSequence(Name);
		if (!LoadAnimSequence(JoinRel(BaseDir, Rel), Seq))
		{
			std::cerr << "SkeletalMeshComponent: failed optional anim '" << Rel << "'\n";
			Seq = UAnimSequence{};
			Seq.Name = Name;
		}
	};
	LoadNamed(Desc.JumpStartAnimRel, "JumpingUp");
	LoadNamed(Desc.FallLoopAnimRel, "FallingIdle");
	LoadNamed(Desc.LandAnimRel, "FallingToLanding");

	for (const auto& Sample : BsDesc.Samples)
	{
		const std::string Key = SequenceKeyFromAnimRel(Sample.AnimRelPath);
		UAnimSequence& Seq = GetOrCreateSequence(Key);
		if (!LoadAnimSequence(JoinRel(BlendDir, Sample.AnimRelPath), Seq))
		{
			std::cerr << "SkeletalMeshComponent: failed anim '" << Sample.AnimRelPath << "'\n";
			return false;
		}
		BlendSpace.AddSample(&Seq, Sample.Position);
	}

	AnimInstance->SetBlendSpace(&BlendSpace);
	AnimInstance->SetSkeleton(&SkeletalMesh->GetSkeleton());
	BindSequencesToAnimInstance();
	ApplyFitHeight(Desc.FitHeight);

	std::cout << "SkeletalMeshComponent: loaded cooked '" << CharacterPath << "' ("
			  << SkeletalMesh->GetSkeleton().BoneCount() << " bones, " << Sequences.size() << " sequences)\n";
	return HasValidMesh();
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
