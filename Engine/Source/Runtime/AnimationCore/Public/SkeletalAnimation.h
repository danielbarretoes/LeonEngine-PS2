#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <string>
#include <vector>


class SkeletalMeshComponent;

constexpr int kMaxSkinBones = 96;
constexpr int kMaxBoneInfluences = 4;

struct FSkeletalVertex {
    glm::vec3 position{};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 texCoord{};
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    glm::ivec4 boneIndices{0};
    glm::vec4 boneWeights{0.0f};
};

struct USkeleton {
    std::vector<std::string> boneNames;
    std::vector<int> parentIndices;         // -1 = root
    std::vector<glm::mat4> inverseBindPose; // cluster geometry_to_bone

    [[nodiscard]] int BoneCount() const { return static_cast<int>(boneNames.size()); }
    [[nodiscard]] int FindBoneIndex(const std::string& name) const;
};

/// Unreal-like UAnimSequence: per-frame bone matrices as model-space `node_to_world`
/// at sample time. Skin matrix = boneWorld * geometry_to_bone (inverse bind).
struct UAnimSequence {
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

/// Unreal-like UBlendSpace1D sample (UAnimSequence + axis position).
struct FBlendSample {
    const UAnimSequence* sequence = nullptr;
    float position = 0.0f;
};

/// Unreal-like UBlendSpace1D: blends adjacent samples along one axis (e.g. Speed).
struct UBlendSpace1D {
    std::string name = "BlendSpace1D";
    float axisMin = 0.0f;
    float axisMax = 1.0f;
    std::vector<FBlendSample> samples;

    void AddSample(const UAnimSequence* sequence, float position) {
        if (sequence == nullptr) {
            return;
        }
        samples.push_back(FBlendSample{sequence, position});
    }

    void ClearSamples() { samples.clear(); }

    /// Resolve axis value into two clips + blend weight toward the higher sample.
    void Evaluate(float axisValue, const UAnimSequence*& outA, const UAnimSequence*& outB,
                  float& outAlpha) const;
};

/// Jump / fall / land clips layered over locomotion (Unreal AnimBP overlay).
struct FAnimJumpClips {
    const UAnimSequence* jumpStart = nullptr;
    const UAnimSequence* fallLoop = nullptr;
    const UAnimSequence* land = nullptr;
};

/// Unreal-like locomotion + jump state machine states.
enum class EAnimJumpState : std::uint8_t {
    Locomotion = 0,
    JumpStart,
    FallLoop,
    Land,
};

/// Unreal-like UAnimInstance base: UBlendSpace1D locomotion only (no jump SM).
/// Pack / Character subclasses add game-specific graphs via `NativeInitializeAnimation`.
class UAnimInstance {
public:
    UAnimInstance() = default;
    virtual ~UAnimInstance() = default;

    UAnimInstance(const UAnimInstance&) = delete;
    UAnimInstance& operator=(const UAnimInstance&) = delete;
    UAnimInstance(UAnimInstance&&) = delete;
    UAnimInstance& operator=(UAnimInstance&&) = delete;

    void SetOwningMeshComponent(SkeletalMeshComponent* owner) { owningMesh_ = owner; }
    [[nodiscard]] SkeletalMeshComponent* GetOwningMeshComponent() const { return owningMesh_; }

    void SetSkeleton(const USkeleton* skeleton) { skeleton_ = skeleton; }
    void SetBlendSpace(const UBlendSpace1D* blendSpace) { blendSpace_ = blendSpace; }

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

    [[nodiscard]] const USkeleton* GetSkeleton() const { return skeleton_; }
    [[nodiscard]] const UBlendSpace1D* GetBlendSpace() const { return blendSpace_; }

private:
    SkeletalMeshComponent* owningMesh_ = nullptr;
    const USkeleton* skeleton_ = nullptr;
    const UBlendSpace1D* blendSpace_ = nullptr;

    float blendInput_ = 0.0f;
    float blendInputTarget_ = 0.0f;
    float locomotionBlendInterpSpeed_ = 0.0f;
    float blendAlpha_ = 0.0f;
    const UAnimSequence* sampleA_ = nullptr;
    const UAnimSequence* sampleB_ = nullptr;
    float timeA_ = 0.0f;
    float timeB_ = 0.0f;
};

/// Framework Character AnimBP: locomotion UBlendSpace1D + Jump/Fall/Land SM (rates pack-tuned).
class UCharacterAnimInstance : public UAnimInstance {
public:
    void SetJumpClips(const FAnimJumpClips& clips) { jumpClips_ = clips; }

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
    struct FPosePlayer {
        const UAnimSequence* sequence = nullptr;
        float time = 0.0f;
    };

    void enterState(EAnimJumpState next);
    void updateJumpStateMachine();
    void samplePlayerBoneWorld(const FPosePlayer& player,
                               std::vector<glm::mat4>& outBoneWorld) const;
    void advancePlayer(FPosePlayer& player, float deltaTime, float playRate) const;
    [[nodiscard]] float playRateForState(EAnimJumpState state) const;

    FAnimJumpClips jumpClips_{};
    EAnimJumpState jumpState_ = EAnimJumpState::Locomotion;
    EAnimJumpState previousState_ = EAnimJumpState::Locomotion;
    FPosePlayer active_{};
    FPosePlayer previous_{};
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

struct FSkeletalMeshData {
    USkeleton skeleton;
    std::vector<FSkeletalVertex> vertices;
    std::vector<std::uint32_t> indices;
    glm::vec3 localMin{0.0f};
    glm::vec3 localMax{0.0f};
    UAnimSequence embeddedAnim;

    [[nodiscard]] bool empty() const { return vertices.empty() || indices.empty(); }
};

[[nodiscard]] bool LoadSkeletalMeshFromFbx(const std::string& path, FSkeletalMeshData& out);
[[nodiscard]] bool LoadAnimSequenceFromFbx(const std::string& path, const USkeleton& skeleton,
                                           UAnimSequence& out);

