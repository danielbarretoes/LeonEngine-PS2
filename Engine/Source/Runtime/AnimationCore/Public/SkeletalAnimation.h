#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace leon {

class SkeletalMeshComponent;

constexpr int kMaxSkinBones = 96;
constexpr int kMaxBoneInfluences = 4;

struct SkeletalVertex {
    glm::vec3 position{};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 texCoord{};
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    glm::ivec4 boneIndices{0};
    glm::vec4 boneWeights{0.0f};
};

struct Skeleton {
    std::vector<std::string> boneNames;
    std::vector<int> parentIndices;         // -1 = root
    std::vector<glm::mat4> inverseBindPose; // cluster geometry_to_bone

    [[nodiscard]] int BoneCount() const { return static_cast<int>(boneNames.size()); }
    [[nodiscard]] int FindBoneIndex(const std::string& name) const;
};

/// Unreal-like AnimSequence: per-frame bone matrices as model-space `node_to_world`
/// at sample time. Skin matrix = boneWorld * geometry_to_bone (inverse bind).
struct AnimSequence {
    std::string name;
    float durationSeconds = 1.0f;
    float framesPerSecond = 30.0f;
    /// When false, SampleLocalPose clamps to the last frame (one-shot Jump / Land).
    bool bLooping = true;
    /// [frame][bone] — model-space bone matrix (`node_to_world`), not parent-local.
    /// Named `localPoseFrames` for historical cooked JSON compatibility.
    std::vector<std::vector<glm::mat4>> localPoseFrames;

    [[nodiscard]] int FrameCount() const { return static_cast<int>(localPoseFrames.size()); }
    [[nodiscard]] bool IsFinished(float timeSeconds) const;
    /// Samples blended bone model-space matrices for `timeSeconds` (loops or clamps by `bLooping`).
    void SampleLocalPose(float timeSeconds, std::vector<glm::mat4>& outBoneWorld) const;
};

/// Unreal-like BlendSpace1D sample (AnimSequence + axis position).
struct BlendSpace1DSample {
    const AnimSequence* sequence = nullptr;
    float position = 0.0f;
};

/// Unreal-like UBlendSpace1D: blends adjacent samples along one axis (e.g. Speed).
struct BlendSpace1D {
    std::string name = "BlendSpace1D";
    float axisMin = 0.0f;
    float axisMax = 1.0f;
    std::vector<BlendSpace1DSample> samples;

    void AddSample(const AnimSequence* sequence, float position) {
        if (sequence == nullptr) {
            return;
        }
        samples.push_back(BlendSpace1DSample{sequence, position});
    }

    void ClearSamples() { samples.clear(); }

    /// Resolve axis value into two clips + blend weight toward the higher sample.
    void Evaluate(float axisValue, const AnimSequence*& outA, const AnimSequence*& outB,
                  float& outAlpha) const;
};

/// Jump / fall / land clips layered over locomotion (Unreal AnimBP overlay).
struct AnimJumpClips {
    const AnimSequence* jumpStart = nullptr;
    const AnimSequence* fallLoop = nullptr;
    const AnimSequence* land = nullptr;
};

/// Unreal-like locomotion + jump state machine states.
enum class EAnimJumpState : std::uint8_t {
    Locomotion = 0,
    JumpStart,
    FallLoop,
    Land,
};

/// Unreal-like UAnimInstance base: BlendSpace1D locomotion only (no jump SM).
/// Pack / Character subclasses add game-specific graphs via `NativeInitializeAnimation`.
class AnimInstance {
public:
    AnimInstance() = default;
    virtual ~AnimInstance() = default;

    AnimInstance(const AnimInstance&) = delete;
    AnimInstance& operator=(const AnimInstance&) = delete;
    AnimInstance(AnimInstance&&) = delete;
    AnimInstance& operator=(AnimInstance&&) = delete;

    void SetOwningMeshComponent(SkeletalMeshComponent* owner) { owningMesh_ = owner; }
    [[nodiscard]] SkeletalMeshComponent* GetOwningMeshComponent() const { return owningMesh_; }

    void SetSkeleton(const Skeleton* skeleton) { skeleton_ = skeleton; }
    void SetBlendSpace(const BlendSpace1D* blendSpace) { blendSpace_ = blendSpace; }

    void SetBlendSpaceInput(float axisValue);
    [[nodiscard]] float GetBlendSpaceInput() const { return blendInput_; }
    [[nodiscard]] float GetBlendSpaceInputTarget() const { return blendInputTarget_; }

    void SetLocomotionBlendInterpSpeed(float speed) {
        locomotionBlendInterpSpeed_ = speed >= 0.0f ? speed : 0.0f;
    }
    [[nodiscard]] float GetLocomotionBlendInterpSpeed() const {
        return locomotionBlendInterpSpeed_;
    }

    virtual void NativeInitializeAnimation() {}
    virtual void NativeUpdateAnimation(float deltaTime);
    /// Current pose bone model-space matrices (`node_to_world`).
    virtual void GetBoneWorldMatrices(std::vector<glm::mat4>& outBoneWorld) const;
    virtual void GetSkinMatrices(std::vector<glm::mat4>& outSkin) const;

    [[nodiscard]] float GetBlendAlpha() const { return blendAlpha_; }

protected:
    void UpdateLocomotion(float deltaTime);
    void SampleLocomotionBoneWorld(std::vector<glm::mat4>& outBoneWorld) const;
    void SkinFromBoneWorld(const std::vector<glm::mat4>& boneWorld,
                           std::vector<glm::mat4>& outSkin) const;

    [[nodiscard]] const Skeleton* GetSkeleton() const { return skeleton_; }
    [[nodiscard]] const BlendSpace1D* GetBlendSpace() const { return blendSpace_; }

private:
    SkeletalMeshComponent* owningMesh_ = nullptr;
    const Skeleton* skeleton_ = nullptr;
    const BlendSpace1D* blendSpace_ = nullptr;

    float blendInput_ = 0.0f;
    float blendInputTarget_ = 0.0f;
    float locomotionBlendInterpSpeed_ = 0.0f;
    float blendAlpha_ = 0.0f;
    const AnimSequence* sampleA_ = nullptr;
    const AnimSequence* sampleB_ = nullptr;
    float timeA_ = 0.0f;
    float timeB_ = 0.0f;
};

/// Framework Character AnimBP: locomotion BlendSpace1D + Jump/Fall/Land SM (rates pack-tuned).
class CharacterAnimInstance : public AnimInstance {
public:
    void SetJumpClips(const AnimJumpClips& clips) { jumpClips_ = clips; }

    void SetCrossfadeDuration(float seconds) {
        crossfadeDuration_ = seconds > 0.0f ? seconds : 0.0f;
    }
    [[nodiscard]] float GetCrossfadeDuration() const { return crossfadeDuration_; }

    void SetLandToLocomotionCrossfade(float seconds) {
        landToLocomotionCrossfade_ = seconds > 0.0f ? seconds : 0.0f;
    }
    [[nodiscard]] float GetLandToLocomotionCrossfade() const { return landToLocomotionCrossfade_; }

    void SetJumpPlayRates(float jumpStart, float fallLoop, float land);
    [[nodiscard]] float GetJumpStartPlayRate() const { return jumpStartPlayRate_; }
    [[nodiscard]] float GetFallLoopPlayRate() const { return fallLoopPlayRate_; }
    [[nodiscard]] float GetLandPlayRate() const { return landPlayRate_; }

    void NotifyJumped();
    void SetMovementState(bool falling, float velocityY, bool justLanded);

    void NativeUpdateAnimation(float deltaTime) override;
    void GetBoneWorldMatrices(std::vector<glm::mat4>& outBoneWorld) const override;
    void GetSkinMatrices(std::vector<glm::mat4>& outSkin) const override;

    [[nodiscard]] EAnimJumpState GetJumpState() const { return jumpState_; }
    [[nodiscard]] float GetCrossfadeAlpha() const { return crossfadeAlpha_; }

private:
    struct PosePlayer {
        const AnimSequence* sequence = nullptr;
        float time = 0.0f;
    };

    void enterState(EAnimJumpState next);
    void updateJumpStateMachine();
    void samplePlayerBoneWorld(const PosePlayer& player,
                               std::vector<glm::mat4>& outBoneWorld) const;
    void advancePlayer(PosePlayer& player, float deltaTime, float playRate) const;
    [[nodiscard]] float playRateForState(EAnimJumpState state) const;

    AnimJumpClips jumpClips_{};
    EAnimJumpState jumpState_ = EAnimJumpState::Locomotion;
    EAnimJumpState previousState_ = EAnimJumpState::Locomotion;
    PosePlayer active_{};
    PosePlayer previous_{};
    float crossfadeDuration_ = 0.15f;
    float crossfadeElapsed_ = 0.0f;
    float crossfadeAlpha_ = 1.0f;
    float activeCrossfadeDuration_ = 0.15f;
    float landToLocomotionCrossfade_ = 0.15f;

    float jumpStartPlayRate_ = 1.0f;
    float fallLoopPlayRate_ = 1.0f;
    float landPlayRate_ = 1.0f;

    bool falling_ = false;
    float velocityY_ = 0.0f;
    bool justLanded_ = false;
    bool jumpRequested_ = false;
};

struct SkeletalMeshData {
    Skeleton skeleton;
    std::vector<SkeletalVertex> vertices;
    std::vector<std::uint32_t> indices;
    glm::vec3 localMin{0.0f};
    glm::vec3 localMax{0.0f};
    AnimSequence embeddedAnim;

    [[nodiscard]] bool empty() const { return vertices.empty() || indices.empty(); }
};

[[nodiscard]] bool LoadSkeletalMeshFromFbx(const std::string& path, SkeletalMeshData& out);
[[nodiscard]] bool LoadAnimSequenceFromFbx(const std::string& path, const Skeleton& skeleton,
                                           AnimSequence& out);

} // namespace leon
