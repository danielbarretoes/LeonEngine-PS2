#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <leon/animation/SkeletalAnimation.h>
#include <leon/core/Transform.h>
#include <leon/gameplay/SceneComponent.h>
#include <leon/render/Material.h>
#include <leon/render/SkeletalMesh.h>
#include <leon/render/StaticMesh.h>
#include <deque>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace leon {

class Engine;
class Renderer;

/// Static mesh glued to a skeletal bone (Unreal-like socket attachment).
struct SkelMeshAttachment {
    std::string boneName;
    std::shared_ptr<StaticMesh> mesh;
    Material material{};
    bool materialOverride = true;
    /// Bone-local TRS applied after the bone model matrix.
    Transform relative{};
    /// When true, `worldMatrixOverride` replaces `component * bone * relative`.
    bool bOverrideWorldMatrix = false;
    glm::mat4 worldMatrixOverride{1.0f};
};

/// Unreal-like USkeletalMeshComponent — SceneComponent with skeletal mesh + AnimInstance.
class SkeletalMeshComponent : public SceneComponent {
public:
    SkeletalMeshComponent();

    void SetSkeletalMesh(std::shared_ptr<SkeletalMesh> mesh);
    [[nodiscard]] SkeletalMesh* GetSkeletalMesh() { return skeletalMesh_.get(); }
    [[nodiscard]] const SkeletalMesh* GetSkeletalMesh() const { return skeletalMesh_.get(); }

    void SetAnimInstance(std::unique_ptr<AnimInstance> instance);
    template <typename TAnim, typename... TArgs>
    TAnim& SetAnimInstance(TArgs&&... args) {
        static_assert(std::is_base_of_v<AnimInstance, TAnim>,
                      "TAnim must derive from leon::AnimInstance");
        auto owned = std::make_unique<TAnim>(std::forward<TArgs>(args)...);
        TAnim& ref = *owned;
        SetAnimInstance(std::move(owned));
        return ref;
    }

    [[nodiscard]] AnimInstance& GetAnimInstance() { return *animInstance_; }
    [[nodiscard]] const AnimInstance& GetAnimInstance() const { return *animInstance_; }

    template <typename TAnim>
    [[nodiscard]] TAnim* GetAnimInstance() {
        return dynamic_cast<TAnim*>(animInstance_.get());
    }
    template <typename TAnim>
    [[nodiscard]] const TAnim* GetAnimInstance() const {
        return dynamic_cast<const TAnim*>(animInstance_.get());
    }

    [[nodiscard]] BlendSpace1D& GetBlendSpace() { return blendSpace_; }
    [[nodiscard]] const BlendSpace1D& GetBlendSpace() const { return blendSpace_; }

    [[nodiscard]] AnimSequence* FindSequence(const std::string& name);
    [[nodiscard]] const AnimSequence* FindSequence(const std::string& name) const;
    [[nodiscard]] AnimSequence& GetOrCreateSequence(const std::string& name);

    /// Bind skeleton/blendspace pointers and call AnimInstance::NativeInitializeAnimation.
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

    /// Bone model-space matrix from the current AnimInstance pose.
    [[nodiscard]] bool GetBoneModelMatrix(const std::string& boneName, glm::mat4& outModel) const;

    /// Component world * bone * attachment.relative (or worldMatrixOverride).
    [[nodiscard]] bool GetAttachmentWorldMatrix(std::size_t attachmentIndex,
                                                glm::mat4& outWorld) const;

    void TickComponent(float deltaTime);
    /// Submit using this component's SceneComponent world transform.
    void SubmitDraw(Renderer& renderer) const;

    [[nodiscard]] bool HasValidMesh() const {
        return skeletalMesh_ != nullptr && skeletalMesh_->Valid();
    }

private:
    void bindAnimInstanceToAssets();

    std::shared_ptr<SkeletalMesh> skeletalMesh_;
    /// Stable storage — BlendSpace / AnimInstance keep raw pointers into these elements.
    std::deque<AnimSequence> sequences_;
    std::unordered_map<std::string, std::size_t> sequenceIndexByName_;
    BlendSpace1D blendSpace_{};
    std::unique_ptr<AnimInstance> animInstance_;
    std::vector<SkelMeshAttachment> attachments_;
    mutable std::vector<glm::mat4> skinMatrices_;
    mutable std::vector<glm::mat4> boneWorldMatrices_;
};

} // namespace leon
