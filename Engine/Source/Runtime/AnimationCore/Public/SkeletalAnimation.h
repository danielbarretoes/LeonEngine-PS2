#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SkeletalAnimation.generated.h"

class USkeletalMeshComponent;

constexpr int32 MaxSkinBones = 96;
constexpr int32 MaxBoneInfluences = 4;

/**
 * Bone matrices (inverse bind, sampled poses, skin) are FMatrix values in UE's row-vector convention and in the
 * engine world (the FBX import converts them with FImportCoordinateConversion), uploaded to the shaders as they are.
 * The skin matrix is InverseBind * BoneWorld.
 */
struct ANIMATIONCORE_API FSkeletalVertex
{
	FVector Position = FVector::ZeroVector;
	FVector Normal = FVector(0.0f, 1.0f, 0.0f);
	FVector2D TexCoord = FVector2D::ZeroVector;
	FVector4 Tangent = FVector4(1.0f, 0.0f, 0.0f, 1.0f);
	FIntVector4 BoneIndices = FIntVector4(0);
	FVector4 BoneWeights = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
};

static_assert(sizeof(FSkeletalVertex) == 80, "FSkeletalVertex is uploaded as an 80-byte interleaved vertex");

struct ANIMATIONCORE_API USkeleton
{
	TArray<FName> BoneNames;
	TArray<int32> ParentIndices; // INDEX_NONE = root
	TArray<FMatrix> InverseBindPose; // cluster geometry_to_bone

	[[nodiscard]] int32 BoneCount() const
	{
		return BoneNames.Num();
	}
	[[nodiscard]] int32 FindBoneIndex(FName InName) const;
};

/**
 * UE-like UAnimSequence: per-frame bone matrices as model-space node_to_world at sample time.
 * Skin matrix = geometry_to_bone (inverse bind) followed by the bone's world matrix.
 */
struct ANIMATIONCORE_API UAnimSequence
{
	FName Name;
	float DurationSeconds = 1.0f;
	float FramesPerSecond = 30.0f;
	/** When false, SampleLocalPose clamps to the last frame (one-shot Jump / Land). */
	bool bLooping = true;
	/** [frame][bone]: model-space bone matrix (node_to_world), not parent-local (historical name). */
	TArray<TArray<FMatrix>> LocalPoseFrames;

	[[nodiscard]] int32 FrameCount() const
	{
		return LocalPoseFrames.Num();
	}
	[[nodiscard]] bool IsFinished(float TimeSeconds) const;
	/** Samples blended bone model-space matrices for TimeSeconds (loops or clamps by bLooping). */
	void SampleLocalPose(float TimeSeconds, TArray<FMatrix>& OutBoneWorld) const;
};

/** UE-like UBlendSpace1D sample (UAnimSequence + axis position). */
struct ANIMATIONCORE_API FBlendSample
{
	const UAnimSequence* Sequence = nullptr;
	float Position = 0.0f;
};

/** UE-like UBlendSpace1D: blends adjacent samples along one axis (e.g. Speed). */
struct ANIMATIONCORE_API UBlendSpace1D
{
	FName Name = FName("BlendSpace1D");
	float AxisMin = 0.0f;
	float AxisMax = 1.0f;
	TArray<FBlendSample> Samples;

	void AddSample(const UAnimSequence* InSequence, float InPosition)
	{
		if (InSequence == nullptr)
		{
			return;
		}
		Samples.Add(FBlendSample{InSequence, InPosition});
	}

	void ClearSamples()
	{
		Samples.Reset();
	}

	/** Resolves the axis value into two clips + a blend weight toward the higher sample. */
	void Evaluate(float AxisValue, const UAnimSequence*& OutA, const UAnimSequence*& OutB, float& OutAlpha) const;
};

/** Jump / fall / land clips layered over locomotion (UE AnimBP overlay). */
struct ANIMATIONCORE_API FAnimJumpClips
{
	const UAnimSequence* JumpStart = nullptr;
	const UAnimSequence* FallLoop = nullptr;
	const UAnimSequence* Land = nullptr;
};

/** UE-like locomotion + jump state machine states. */
enum class EAnimJumpState : uint8
{
	Locomotion = 0,
	JumpStart,
	FallLoop,
	Land,
};

/**
 * UE-like UAnimInstance base: UBlendSpace1D locomotion only (no jump state machine).
 * Game / character subclasses add game-specific graphs through NativeInitializeAnimation.
 */
UCLASS(Transient)
class ANIMATIONCORE_API UAnimInstance : public UObject
{
	GENERATED_BODY()

public:
	UAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetOwningMeshComponent(USkeletalMeshComponent* Owner)
	{
		OwningMesh = Owner;
	}
	[[nodiscard]] USkeletalMeshComponent* GetOwningMeshComponent() const
	{
		return OwningMesh;
	}

	void SetSkeleton(const USkeleton* InSkeleton)
	{
		Skeleton = InSkeleton;
	}
	void SetBlendSpace(const UBlendSpace1D* InBlendSpace)
	{
		BlendSpace = InBlendSpace;
	}

	void SetBlendSpaceInput(float AxisValue);
	[[nodiscard]] float GetBlendSpaceInput() const
	{
		return BlendInput;
	}
	[[nodiscard]] float GetBlendSpaceInputTarget() const
	{
		return BlendInputTarget;
	}

	void SetLocomotionBlendInterpSpeed(float Speed)
	{
		LocomotionBlendInterpSpeed = Speed >= 0.0f ? Speed : 0.0f;
	}
	[[nodiscard]] float GetLocomotionBlendInterpSpeed() const
	{
		return LocomotionBlendInterpSpeed;
	}

	virtual void NativeInitializeAnimation()
	{
	}
	virtual void NativeUpdateAnimation(float DeltaTime);
	/** Current pose bone model-space matrices (node_to_world). */
	virtual void GetBoneWorldMatrices(TArray<FMatrix>& OutBoneWorld) const;
	virtual void GetSkinMatrices(TArray<FMatrix>& OutSkin) const;

	[[nodiscard]] float GetBlendAlpha() const
	{
		return BlendAlpha;
	}

protected:
	void UpdateLocomotion(float DeltaTime);
	void SampleLocomotionBoneWorld(TArray<FMatrix>& OutBoneWorld) const;
	void SkinFromBoneWorld(const TArray<FMatrix>& BoneWorld, TArray<FMatrix>& OutSkin) const;

	[[nodiscard]] const USkeleton* GetSkeleton() const
	{
		return Skeleton;
	}
	[[nodiscard]] const UBlendSpace1D* GetBlendSpace() const
	{
		return BlendSpace;
	}

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

/** Framework character AnimBP: locomotion UBlendSpace1D + Jump / Fall / Land state machine (rates game-tuned). */
UCLASS(Transient)
class ANIMATIONCORE_API UCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UCharacterAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetJumpClips(const FAnimJumpClips& Clips)
	{
		JumpClips = Clips;
	}

	void SetCrossfadeDuration(float Seconds)
	{
		CrossfadeDuration = Seconds > 0.0f ? Seconds : 0.0f;
	}
	[[nodiscard]] float GetCrossfadeDuration() const
	{
		return CrossfadeDuration;
	}

	void SetLandToLocomotionCrossfade(float Seconds)
	{
		LandToLocomotionCrossfade = Seconds > 0.0f ? Seconds : 0.0f;
	}
	[[nodiscard]] float GetLandToLocomotionCrossfade() const
	{
		return LandToLocomotionCrossfade;
	}

	void SetJumpPlayRates(float InJumpStart, float InFallLoop, float InLand);
	[[nodiscard]] float GetJumpStartPlayRate() const
	{
		return JumpStartPlayRate;
	}
	[[nodiscard]] float GetFallLoopPlayRate() const
	{
		return FallLoopPlayRate;
	}
	[[nodiscard]] float GetLandPlayRate() const
	{
		return LandPlayRate;
	}

	void NotifyJumped();
	void SetMovementState(bool bInFalling, float InVelocityZ, bool bInJustLanded);

	void NativeUpdateAnimation(float DeltaTime) override;
	void GetBoneWorldMatrices(TArray<FMatrix>& OutBoneWorld) const override;
	void GetSkinMatrices(TArray<FMatrix>& OutSkin) const override;

	[[nodiscard]] EAnimJumpState GetJumpState() const
	{
		return JumpState;
	}
	[[nodiscard]] float GetCrossfadeAlpha() const
	{
		return CrossfadeAlpha;
	}

private:
	struct FPosePlayer
	{
		const UAnimSequence* Sequence = nullptr;
		float Time = 0.0f;
	};

	void EnterState(EAnimJumpState Next);
	void UpdateJumpStateMachine();
	void SamplePlayerBoneWorld(const FPosePlayer& Player, TArray<FMatrix>& OutBoneWorld) const;
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
	float VelocityZ = 0.0f;
	bool bJustLanded = false;
	bool bJumpRequested = false;
};

struct ANIMATIONCORE_API FSkeletalMeshData
{
	USkeleton Skeleton;
	TArray<FSkeletalVertex> Vertices;
	TArray<uint32> Indices;
	FVector LocalMin = FVector::ZeroVector;
	FVector LocalMax = FVector::ZeroVector;
	UAnimSequence EmbeddedAnim;

	[[nodiscard]] bool IsEmpty() const
	{
		return Vertices.Num() == 0 || Indices.Num() == 0;
	}
};
