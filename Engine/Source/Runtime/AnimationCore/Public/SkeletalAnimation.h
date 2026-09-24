#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <string>
#include <vector>


class USkeletalMeshComponent;

constexpr int MaxSkinBones = 96;
constexpr int MaxBoneInfluences = 4;

struct FSkeletalVertex {
    glm::vec3 Position{};
    glm::vec3 Normal{0.0f, 1.0f, 0.0f};
    glm::vec2 TexCoord{};
    glm::vec4 Tangent{1.0f, 0.0f, 0.0f, 1.0f};
    glm::ivec4 BoneIndices{0};
    glm::vec4 BoneWeights{0.0f};
};

struct USkeleton {
    std::vector<std::string> BoneNames;
    std::vector<int> ParentIndices;         // -1 = root
    std::vector<glm::mat4> InverseBindPose; // cluster geometry_to_bone

    [[nodiscard]] int BoneCount() const { return static_cast<int>(BoneNames.size()); }
    [[nodiscard]] int FindBoneIndex(const std::string& InName) const;
};

/// Unreal-like UAnimSequence: per-frame bone matrices as model-space `node_to_world`
/// at sample time. Skin matrix = boneWorld * geometry_to_bone (inverse bind).
struct UAnimSequence {
    std::string Name;
    float DurationSeconds = 1.0f;
    float FramesPerSecond = 30.0f;
    /// When false, SampleLocalPose clamps to the last frame (one-shot Jump / Land).
    bool bLooping = true;
    /// [frame][bone] — model-space bone matrix (`node_to_world`), not parent-local.
    /// Named `localPoseFrames` for historical cooked JSON compatibility.
    std::vector<std::vector<glm::mat4>> LocalPoseFrames;

    [[nodiscard]] int FrameCount() const { return static_cast<int>(LocalPoseFrames.size()); }
    [[nodiscard]] bool IsFinished(float TimeSeconds) const;
    /// Samples blended bone model-space matrices for `timeSeconds` (loops or clamps by `bLooping`).
    void SampleLocalPose(float TimeSeconds, std::vector<glm::mat4>& OutBoneWorld) const;
};

/// Unreal-like UBlendSpace1D sample (UAnimSequence + axis position).
struct FBlendSample {
    const UAnimSequence* Sequence = nullptr;
    float Position = 0.0f;
};

/// Unreal-like UBlendSpace1D: blends adjacent samples along one axis (e.g. Speed).
struct UBlendSpace1D {
    std::string Name = "BlendSpace1D";
    float AxisMin = 0.0f;
    float AxisMax = 1.0f;
    std::vector<FBlendSample> Samples;

    void AddSample(const UAnimSequence* InSequence, float InPosition) {
        if (InSequence == nullptr) {
            return;
        }
        Samples.push_back(FBlendSample{InSequence, InPosition});
    }

    void ClearSamples() { Samples.clear(); }

    /// Resolve axis value into two clips + blend weight toward the higher sample.
    void Evaluate(float AxisValue, const UAnimSequence*& OutA, const UAnimSequence*& OutB,
                  float& OutAlpha) const;
};

/// Jump / fall / land clips layered over locomotion (Unreal AnimBP overlay).
struct FAnimJumpClips {
    const UAnimSequence* JumpStart = nullptr;
    const UAnimSequence* FallLoop = nullptr;
    const UAnimSequence* Land = nullptr;
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

    void SetOwningMeshComponent(USkeletalMeshComponent* Owner) { OwningMesh = Owner; }
    [[nodiscard]] USkeletalMeshComponent* GetOwningMeshComponent() const { return OwningMesh; }

    void SetSkeleton(const USkeleton* InSkeleton) { Skeleton = InSkeleton; }
    void SetBlendSpace(const UBlendSpace1D* InBlendSpace) { BlendSpace = InBlendSpace; }

    void SetBlendSpaceInput(float AxisValue);
    [[nodiscard]] float GetBlendSpaceInput() const { return BlendInput; }
    [[nodiscard]] float GetBlendSpaceInputTarget() const { return BlendInputTarget; }

    void SetLocomotionBlendInterpSpeed(float Speed) {
        LocomotionBlendInterpSpeed = Speed >= 0.0f ? Speed : 0.0f;
    }
    [[nodiscard]] float GetLocomotionBlendInterpSpeed() const {
        return LocomotionBlendInterpSpeed;
    }

    virtual void NativeInitializeAnimation() {}
    virtual void NativeUpdateAnimation(float DeltaTime);
    /// Current pose bone model-space matrices (`node_to_world`).
    virtual void GetBoneWorldMatrices(std::vector<glm::mat4>& OutBoneWorld) const;
    virtual void GetSkinMatrices(std::vector<glm::mat4>& OutSkin) const;

    [[nodiscard]] float GetBlendAlpha() const { return BlendAlpha; }

protected:
    void UpdateLocomotion(float DeltaTime);
    void SampleLocomotionBoneWorld(std::vector<glm::mat4>& OutBoneWorld) const;
    void SkinFromBoneWorld(const std::vector<glm::mat4>& BoneWorld,
                           std::vector<glm::mat4>& OutSkin) const;

    [[nodiscard]] const USkeleton* GetSkeleton() const { return Skeleton; }
    [[nodiscard]] const UBlendSpace1D* GetBlendSpace() const { return BlendSpace; }

private:
    USkeletalMeshComponent* OwningMesh = nullptr;
    const USkeleton* Skeleton = nullptr;
    const UBlendSpace1D* BlendSpace = nullptr;

    float BlendInput = 0.0f;
    float BlendInputTarget = 0.0f;
    float LocomotionBlendInterpSpeed = 0.0f;
    float BlendAlpha = 0.0f;
    const UAnimSequence* SampleA = nullptr;
    const UAnimSequence* SampleB = nullptr;
    float TimeA = 0.0f;
    float TimeB = 0.0f;
};

/// Framework Character AnimBP: locomotion UBlendSpace1D + Jump/Fall/Land SM (rates pack-tuned).
class UCharacterAnimInstance : public UAnimInstance {
public:
    void SetJumpClips(const FAnimJumpClips& Clips) { JumpClips = Clips; }

    void SetCrossfadeDuration(float Seconds) {
        CrossfadeDuration = Seconds > 0.0f ? Seconds : 0.0f;
    }
    [[nodiscard]] float GetCrossfadeDuration() const { return CrossfadeDuration; }

    void SetLandToLocomotionCrossfade(float Seconds) {
        LandToLocomotionCrossfade = Seconds > 0.0f ? Seconds : 0.0f;
    }
    [[nodiscard]] float GetLandToLocomotionCrossfade() const { return LandToLocomotionCrossfade; }

    void SetJumpPlayRates(float InJumpStart, float InFallLoop, float InLand);
    [[nodiscard]] float GetJumpStartPlayRate() const { return JumpStartPlayRate; }
    [[nodiscard]] float GetFallLoopPlayRate() const { return FallLoopPlayRate; }
    [[nodiscard]] float GetLandPlayRate() const { return LandPlayRate; }

    void NotifyJumped();
    void SetMovementState(bool bInFalling, float InVelocityY, bool bInJustLanded);

    void NativeUpdateAnimation(float DeltaTime) override;
    void GetBoneWorldMatrices(std::vector<glm::mat4>& OutBoneWorld) const override;
    void GetSkinMatrices(std::vector<glm::mat4>& OutSkin) const override;

    [[nodiscard]] EAnimJumpState GetJumpState() const { return JumpState; }
    [[nodiscard]] float GetCrossfadeAlpha() const { return CrossfadeAlpha; }

private:
    struct FPosePlayer {
        const UAnimSequence* Sequence = nullptr;
        float Time = 0.0f;
    };

    void EnterState(EAnimJumpState Next);
    void UpdateJumpStateMachine();
    void SamplePlayerBoneWorld(const FPosePlayer& Player,
                               std::vector<glm::mat4>& OutBoneWorld) const;
    void AdvancePlayer(FPosePlayer& Player, float DeltaTime, float PlayRate) const;
    [[nodiscard]] float PlayRateForState(EAnimJumpState State) const;

    FAnimJumpClips JumpClips{};
    EAnimJumpState JumpState = EAnimJumpState::Locomotion;
    EAnimJumpState PreviousState = EAnimJumpState::Locomotion;
    FPosePlayer Active{};
    FPosePlayer Previous{};
    float CrossfadeDuration = 0.15f;
    float CrossfadeElapsed = 0.0f;
    float CrossfadeAlpha = 1.0f;
    float ActiveCrossfadeDuration = 0.15f;
    float LandToLocomotionCrossfade = 0.15f;

    float JumpStartPlayRate = 1.0f;
    float FallLoopPlayRate = 1.0f;
    float LandPlayRate = 1.0f;

    bool bFalling = false;
    float VelocityY = 0.0f;
    bool bJustLanded = false;
    bool bJumpRequested = false;
};

struct FSkeletalMeshData {
    USkeleton Skeleton;
    std::vector<FSkeletalVertex> Vertices;
    std::vector<std::uint32_t> Indices;
    glm::vec3 LocalMin{0.0f};
    glm::vec3 LocalMax{0.0f};
    UAnimSequence EmbeddedAnim;

    [[nodiscard]] bool empty() const { return Vertices.empty() || Indices.empty(); }
};

[[nodiscard]] bool LoadSkeletalMeshFromFbx(const std::string& Path, FSkeletalMeshData& Out);
[[nodiscard]] bool LoadAnimSequenceFromFbx(const std::string& Path, const USkeleton& InSkeleton,
                                           UAnimSequence& Out);

