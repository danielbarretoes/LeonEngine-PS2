#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "SkeletalAnimation.h"
#include "Math/Transform.h"
#include "Components/SceneComponent.h"
#include "Material.h"
#include "SkeletalMesh.h"
#include "StaticMesh.h"
#include <deque>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>


class Engine;
class FSceneRenderer;

/// Static mesh glued to a skeletal bone (Unreal-like socket attachment).
struct SkelMeshAttachment {
    std::string boneName;
    std::shared_ptr<UStaticMesh> mesh;
    FMaterial material{};
    bool materialOverride = true;
    /// Bone-local TRS applied after the bone model matrix.
    FTransform relative{};
    /// When true, `worldMatrixOverride` replaces `component * bone * relative`.
    bool bOverrideWorldMatrix = false;
    glm::mat4 worldMatrixOverride{1.0f};
};

/// Unreal-like USkeletalMeshComponent — SceneComponent with skeletal mesh + UAnimInstance.
class SkeletalMeshComponent : public SceneComponent {
public:
    SkeletalMeshComponent();

    void SetSkeletalMesh(std::shared_ptr<USkeletalMesh> mesh);
    [[nodiscard]] USkeletalMesh* GetSkeletalMesh() { return skeletalMesh_.get(); }
    [[nodiscard]] const USkeletalMesh* GetSkeletalMesh() const { return skeletalMesh_.get(); }

    void SetAnimInstance(std::unique_ptr<UAnimInstance> instance);
    template <typename TAnim, typename... TArgs>
    TAnim& SetAnimInstance(TArgs&&... args) {
        static_assert(std::is_base_of_v<UAnimInstance, TAnim>,
                      "TAnim must derive from AnimInstance");
        auto owned = std::make_unique<TAnim>(std::forward<TArgs>(args)...);
        TAnim& ref = *owned;
        SetAnimInstance(std::move(owned));
        return ref;
    }

    [[nodiscard]] UAnimInstance& GetAnimInstance() { return *animInstance_; }
    [[nodiscard]] const UAnimInstance& GetAnimInstance() const { return *animInstance_; }

    template <typename TAnim>
    [[nodiscard]] TAnim* GetAnimInstance() {
        return dynamic_cast<TAnim*>(animInstance_.get());
    }
    template <typename TAnim>
    [[nodiscard]] const TAnim* GetAnimInstance() const {
        return dynamic_cast<const TAnim*>(animInstance_.get());
    }

    [[nodiscard]] UBlendSpace1D& GetBlendSpace() { return blendSpace_; }
    [[nodiscard]] const UBlendSpace1D& GetBlendSpace() const { return blendSpace_; }

    [[nodiscard]] UAnimSequence* FindSequence(const std::string& name);
    [[nodiscard]] const UAnimSequence* FindSequence(const std::string& name) const;
    [[nodiscard]] UAnimSequence& GetOrCreateSequence(const std::string& name);

    /// Bind skeleton/blendspace pointers and call UAnimInstance::NativeInitializeAnimation.
    void BindSequencesToAnimInstance();

    void ApplyFitHeight(float fitHeight);

    [[nodiscard]] bool LoadFromFbx(const std::string& meshFbxPath, const std::string& runFbxPath,
                                   float fitHeight);
    /// Load a Leon character package (`.lchar`) or legacy `.character.json`.
    [[nodiscard]] bool LoadFromCooked(Engine& engine, const std::string& characterAssetPath);

    void ClearAttachments();
    SkelMeshAttachment& AddAttachment(SkelMeshAttachment attachment);
    [[nodiscard]] std::vector<SkelMeshAttachment>& Attachments() { return attachments_; }
    [[nodiscard]] const std::vector<SkelMeshAttachment>& Attachments() const {
        return attachments_;
    }

    /// Bone model-space matrix from the current UAnimInstance pose.
    [[nodiscard]] bool GetBoneModelMatrix(const std::string& boneName, glm::mat4& outModel) const;

    /// Component world * bone * attachment.relative (or worldMatrixOverride).
    [[nodiscard]] bool GetAttachmentWorldMatrix(std::size_t attachmentIndex,
                                                glm::mat4& outWorld) const;

    void TickComponent(float deltaTime);
    /// Submit using this component's SceneComponent world transform.
    void SubmitDraw(FSceneRenderer& renderer) const;

    [[nodiscard]] bool HasValidMesh() const {
        return skeletalMesh_ != nullptr && skeletalMesh_->Valid();
    }

private:
    void bindAnimInstanceToAssets();

    std::shared_ptr<USkeletalMesh> skeletalMesh_;
    /// Stable storage — BlendSpace / UAnimInstance keep raw pointers into these elements.
    std::deque<UAnimSequence> sequences_;
    std::unordered_map<std::string, std::size_t> sequenceIndexByName_;
    UBlendSpace1D blendSpace_{};
    std::unique_ptr<UAnimInstance> animInstance_;
    std::vector<SkelMeshAttachment> attachments_;
    mutable std::vector<glm::mat4> skinMatrices_;
    mutable std::vector<glm::mat4> boneWorldMatrices_;
};

