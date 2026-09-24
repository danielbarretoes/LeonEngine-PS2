#include <glm/common.hpp>
#include <glm/vec3.hpp>

#include <filesystem>
#include <iostream>
#include "Animation/CookedSkeletal.h"
#include "Misc/Paths.h"
#include "Engine/GameEngine.h"
#include "Components/SkeletalMeshComponent.h"
#include "SceneRenderer.h"
#include "StaticMesh.h"

namespace {

namespace fs = std::filesystem;

[[nodiscard]] std::string joinRel(const std::string& baseDir, const std::string& rel) {
    return (fs::path(baseDir) / rel).lexically_normal().string();
}

[[nodiscard]] std::string sequenceKeyFromAnimRel(const std::string& animRel) {
    std::string key = fs::path(animRel).filename().string();
    const auto dot = key.find('.');
    if (dot != std::string::npos) {
        key.resize(dot);
    }
    return key;
}

} // namespace

USkeletalMeshComponent::USkeletalMeshComponent() : animInstance_(std::make_unique<UAnimInstance>()) {
    animInstance_->SetOwningMeshComponent(this);
}

void USkeletalMeshComponent::SetAnimInstance(std::unique_ptr<UAnimInstance> instance) {
    animInstance_ = std::move(instance);
    if (animInstance_ == nullptr) {
        animInstance_ = std::make_unique<UAnimInstance>();
    }
    animInstance_->SetOwningMeshComponent(this);
    bindAnimInstanceToAssets();
}

void USkeletalMeshComponent::bindAnimInstanceToAssets() {
    if (skeletalMesh_ != nullptr && skeletalMesh_->Valid()) {
        animInstance_->SetSkeleton(&skeletalMesh_->GetSkeleton());
    } else {
        animInstance_->SetSkeleton(nullptr);
    }
    if (!blendSpace_.samples.empty()) {
        animInstance_->SetBlendSpace(&blendSpace_);
    }
}

UAnimSequence* USkeletalMeshComponent::FindSequence(const std::string& name) {
    const auto it = sequenceIndexByName_.find(name);
    return it != sequenceIndexByName_.end() ? &sequences_[it->second] : nullptr;
}

const UAnimSequence* USkeletalMeshComponent::FindSequence(const std::string& name) const {
    const auto it = sequenceIndexByName_.find(name);
    return it != sequenceIndexByName_.end() ? &sequences_[it->second] : nullptr;
}

UAnimSequence& USkeletalMeshComponent::GetOrCreateSequence(const std::string& name) {
    if (const auto it = sequenceIndexByName_.find(name); it != sequenceIndexByName_.end()) {
        return sequences_[it->second];
    }
    sequences_.push_back(UAnimSequence{});
    sequences_.back().name = name;
    sequenceIndexByName_[name] = sequences_.size() - 1;
    return sequences_.back();
}

void USkeletalMeshComponent::BindSequencesToAnimInstance() {
    bindAnimInstanceToAssets();
    animInstance_->NativeInitializeAnimation();
}

void USkeletalMeshComponent::SetSkeletalMesh(std::shared_ptr<USkeletalMesh> mesh) {
    skeletalMesh_ = std::move(mesh);
    if (skeletalMesh_ != nullptr && skeletalMesh_->Valid()) {
        animInstance_->SetSkeleton(&skeletalMesh_->GetSkeleton());
    } else {
        animInstance_->SetSkeleton(nullptr);
    }
}

void USkeletalMeshComponent::ApplyFitHeight(float fitHeight) {
    if (!HasValidMesh() || fitHeight <= 0.0f) {
        return;
    }
    const float scale = skeletalMesh_->FitUniformScale(fitHeight);
    constexpr float kGroundEpsilon = 0.008f;
    const glm::vec3 mn = skeletalMesh_->LocalMin();
    const glm::vec3 mx = skeletalMesh_->LocalMax();
    const glm::vec3 center = (mn + mx) * 0.5f;
    RelativeScale = {scale, scale, scale};
    RelativeLocation = {(-center.x) * scale, ((-mn.y) * scale) + kGroundEpsilon,
                        (-center.z) * scale};
}

void USkeletalMeshComponent::ClearAttachments() {
    attachments_.clear();
}

FSkelMeshAttachment& USkeletalMeshComponent::AddAttachment(FSkelMeshAttachment attachment) {
    attachments_.push_back(std::move(attachment));
    return attachments_.back();
}

bool USkeletalMeshComponent::GetBoneModelMatrix(const std::string& boneName,
                                               glm::mat4& outModel) const {
    if (!HasValidMesh() || boneName.empty()) {
        return false;
    }
    const int boneIndex = skeletalMesh_->GetSkeleton().FindBoneIndex(boneName);
    if (boneIndex < 0) {
        return false;
    }
    animInstance_->GetBoneWorldMatrices(boneWorldMatrices_);
    if (boneWorldMatrices_.size() <= static_cast<std::size_t>(boneIndex)) {
        return false;
    }
    outModel = boneWorldMatrices_[static_cast<std::size_t>(boneIndex)];
    return true;
}

bool USkeletalMeshComponent::GetAttachmentWorldMatrix(std::size_t attachmentIndex,
                                                     glm::mat4& outWorld) const {
    if (attachmentIndex >= attachments_.size()) {
        return false;
    }
    const FSkelMeshAttachment& att = attachments_[attachmentIndex];
    if (att.bOverrideWorldMatrix) {
        outWorld = att.worldMatrixOverride;
        return true;
    }
    glm::mat4 boneModel{};
    if (!GetBoneModelMatrix(att.boneName, boneModel)) {
        return false;
    }
    outWorld = GetComponentTransform() * boneModel * att.relative.ModelMatrix();
    return true;
}

bool USkeletalMeshComponent::LoadFromFbx(const std::string& meshFbxPath,
                                        const std::string& runFbxPath, float fitHeight) {
    FSkeletalMeshData data;
    const std::string meshPath = FPaths::ResolveAssetPath(meshFbxPath);
    if (!LoadSkeletalMeshFromFbx(meshPath, data)) {
        std::cerr << "SkeletalMeshComponent: failed to load '" << meshPath << "'\n";
        return false;
    }

    sequences_.clear();
    sequenceIndexByName_.clear();
    UAnimSequence& idle = GetOrCreateSequence("BreathingIdle");
    idle = std::move(data.embeddedAnim);
    idle.name = "BreathingIdle";
    if (idle.FrameCount() <= 0) {
        std::cerr << "SkeletalMeshComponent: mesh FBX has no embedded AnimSequence\n";
    }

    auto mesh = std::make_shared<USkeletalMesh>(USkeletalMesh::Upload(std::move(data)));
    if (mesh == nullptr || !mesh->Valid()) {
        std::cerr << "SkeletalMeshComponent: GPU upload failed\n";
        return false;
    }
    mesh->GetMaterial().albedo = {0.72f, 0.74f, 0.78f};
    mesh->GetMaterial().shininess = 24.0f;
    mesh->GetMaterial().syncRoughnessFromShininess();

    UAnimSequence& run = GetOrCreateSequence("Running");
    const std::string runPath = FPaths::ResolveAssetPath(runFbxPath);
    if (!LoadAnimSequenceFromFbx(runPath, mesh->GetSkeleton(), run)) {
        std::cerr << "SkeletalMeshComponent: failed to load run AnimSequence '" << runPath << "'\n";
    }
    run.name = "Running";

    SetSkeletalMesh(std::move(mesh));
    ApplyFitHeight(fitHeight);
    BindSequencesToAnimInstance();

    std::cout << "SkeletalMeshComponent: loaded FBX (" << skeletalMesh_->GetSkeleton().BoneCount()
              << " bones)\n";
    return true;
}

bool USkeletalMeshComponent::LoadFromCooked(UGameEngine& engine, const std::string& characterAssetPath) {
    const std::string characterPath = FPaths::ResolveAssetPath(characterAssetPath);
    FCharacterVisualDesc desc;
    if (!LoadCharacterVisual(characterPath, desc)) {
        std::cerr << "SkeletalMeshComponent: failed to load character '" << characterPath << "'\n";
        return false;
    }

    const std::string baseDir = fs::path(characterPath).parent_path().string();
    const std::string meshJson = joinRel(baseDir, desc.skeletalMeshRel);
    const std::string blendJson = joinRel(baseDir, desc.blendSpaceRel);

    FSkeletalMeshData meshData;
    std::string materialRel;
    if (!LoadSkeletalMesh(meshJson, meshData, nullptr, &materialRel)) {
        std::cerr << "SkeletalMeshComponent: failed cooked skelmesh '" << meshJson << "'\n";
        return false;
    }

    auto mesh = std::make_shared<USkeletalMesh>(engine.GetResources().IsGpuUploadEnabled()
                                                   ? USkeletalMesh::Upload(std::move(meshData))
                                                   : USkeletalMesh::CreateCpu(std::move(meshData)));
    if (mesh == nullptr || !mesh->Valid()) {
        std::cerr << "SkeletalMeshComponent: cooked mesh create failed\n";
        return false;
    }
    if (!materialRel.empty()) {
        const std::string matPath = joinRel(fs::path(meshJson).parent_path().string(), materialRel);
        mesh->SetMaterial(engine.GetResources().LoadMaterial(matPath));
    } else {
        mesh->GetMaterial().albedo = {0.72f, 0.74f, 0.78f};
        mesh->GetMaterial().shininess = 24.0f;
        mesh->GetMaterial().syncRoughnessFromShininess();
    }
    SetSkeletalMesh(std::move(mesh));

    FBlendSpace1DAssetDesc bsDesc;
    if (!LoadBlendSpace1DJson(blendJson, bsDesc) || bsDesc.samples.empty()) {
        std::cerr << "SkeletalMeshComponent: failed blendspace '" << blendJson << "'\n";
        return false;
    }

    sequences_.clear();
    sequenceIndexByName_.clear();
    const std::string blendDir = fs::path(blendJson).parent_path().string();
    blendSpace_.name = bsDesc.name;
    blendSpace_.axisMin = bsDesc.axisMin;
    blendSpace_.axisMax = bsDesc.axisMax;
    blendSpace_.ClearSamples();

    // Optional jump clips first; deque keeps pointers stable across later inserts.
    auto loadNamed = [&](const std::string& rel, const char* fallbackKey) {
        if (rel.empty()) {
            return;
        }
        const std::string key = sequenceKeyFromAnimRel(rel);
        const std::string name = key.empty() ? fallbackKey : key;
        UAnimSequence& seq = GetOrCreateSequence(name);
        if (!LoadAnimSequence(joinRel(baseDir, rel), seq)) {
            std::cerr << "SkeletalMeshComponent: failed optional anim '" << rel << "'\n";
            seq = UAnimSequence{};
            seq.name = name;
        }
    };
    loadNamed(desc.jumpStartAnimRel, "JumpingUp");
    loadNamed(desc.fallLoopAnimRel, "FallingIdle");
    loadNamed(desc.landAnimRel, "FallingToLanding");

    for (const auto& sample : bsDesc.samples) {
        const std::string key = sequenceKeyFromAnimRel(sample.animRelPath);
        UAnimSequence& seq = GetOrCreateSequence(key);
        if (!LoadAnimSequence(joinRel(blendDir, sample.animRelPath), seq)) {
            std::cerr << "SkeletalMeshComponent: failed anim '" << sample.animRelPath << "'\n";
            return false;
        }
        blendSpace_.AddSample(&seq, sample.position);
    }

    animInstance_->SetBlendSpace(&blendSpace_);
    animInstance_->SetSkeleton(&skeletalMesh_->GetSkeleton());
    BindSequencesToAnimInstance();
    ApplyFitHeight(desc.fitHeight);

    std::cout << "SkeletalMeshComponent: loaded cooked '" << characterPath << "' ("
              << skeletalMesh_->GetSkeleton().BoneCount() << " bones, " << sequences_.size()
              << " sequences)\n";
    return HasValidMesh();
}

void USkeletalMeshComponent::TickComponent(float deltaTime) {
    if (!HasValidMesh()) {
        return;
    }
    animInstance_->NativeUpdateAnimation(deltaTime);
}

void USkeletalMeshComponent::SubmitDraw(FSceneRenderer& renderer) const {
    if (!HasValidMesh()) {
        return;
    }

    animInstance_->GetSkinMatrices(skinMatrices_);
    renderer.SubmitSkeletalDraw(*skeletalMesh_, GetComponentTransform(), skinMatrices_);

    for (std::size_t i = 0; i < attachments_.size(); ++i) {
        const FSkelMeshAttachment& att = attachments_[i];
        if (att.mesh == nullptr || !att.mesh->Valid()) {
            continue;
        }
        glm::mat4 attachmentWorld{};
        if (!GetAttachmentWorldMatrix(i, attachmentWorld)) {
            continue;
        }
        renderer.SubmitStaticDraw(*att.mesh, attachmentWorld, att.material);
    }
}

