#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"
#include "CharacterAnimInstance.generated.h"

/** Jump / fall / land clips layered over locomotion (UE AnimBP overlay). */
USTRUCT()
struct ENGINE_API FAnimJumpClips
{
	GENERATED_BODY()

	UPROPERTY()
	const UAnimSequence* JumpStart = nullptr;

	UPROPERTY()
	const UAnimSequence* FallLoop = nullptr;

	UPROPERTY()
	const UAnimSequence* Land = nullptr;
};

/** A clip being played and its time (the jump state machine's players). */
USTRUCT()
struct ENGINE_API FAnimPosePlayer
{
	GENERATED_BODY()

	UPROPERTY()
	const UAnimSequence* Sequence = nullptr;

	float Time = 0.0f;
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
 * Framework character AnimBP: the locomotion blend space + a Jump / Fall / Land state machine (rates game-tuned) as the
 * base pose, under UAnimInstance's slots and aim offset. The active state's clip fires its notifies.
 */
UCLASS(Transient)
class ENGINE_API UCharacterAnimInstance : public UAnimInstance
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

	/**
	 * The crouched locomotion (UE AnimBP: a crouch state with its own blend space): while crouched the locomotion
	 * plays InBlendSpace with the same input instead of the standing one (SetBlendSpace), crossfading over
	 * CrossfadeDuration when the crouch changes. Null: the standing one always.
	 */
	void SetCrouchBlendSpace(const UBlendSpaceBase* InBlendSpace)
	{
		CrouchBlendSpace = InBlendSpace;
	}
	void SetCrouched(bool bInCrouched)
	{
		bCrouched = bInCrouched;
	}
	[[nodiscard]] bool IsCrouched() const
	{
		return bCrouched;
	}
	/** The weight of the crouched locomotion now, 0 standing to 1 crouched. */
	[[nodiscard]] float GetCrouchAlpha() const
	{
		return CrouchAlpha;
	}

	void NativeUpdateAnimation(float DeltaTime) override;

	[[nodiscard]] EAnimJumpState GetJumpState() const
	{
		return JumpState;
	}
	[[nodiscard]] float GetCrossfadeAlpha() const
	{
		return CrossfadeAlpha;
	}

protected:
	/** The active state's pose, crossfading from the previous state's in local space. */
	void EvaluateBasePose(TArrayView<FTransform> OutPose) const override;

private:
	void EnterState(EAnimJumpState Next);
	void UpdateJumpStateMachine();
	void SamplePlayerPose(const FAnimPosePlayer& Player, TArrayView<FTransform> OutPose) const;
	/** The locomotion with the crouch crossfade: the active space's samples and, while fading, the other's. */
	void SampleCrouchedLocomotionPose(TArrayView<FTransform> OutPose) const;
	/** Picks the standing or crouched space for the update and eases CrouchAlpha. */
	void UpdateCrouch(float DeltaTime);
	[[nodiscard]] float PlayRateForState(EAnimJumpState State) const;

	UPROPERTY(Transient)
	FAnimJumpClips JumpClips;

	EAnimJumpState JumpState = EAnimJumpState::Locomotion;
	EAnimJumpState PreviousState = EAnimJumpState::Locomotion;

	UPROPERTY(Transient)
	FAnimPosePlayer Active;

	UPROPERTY(Transient)
	FAnimPosePlayer Previous;

	float CrossfadeDuration = 0.15f;
	float CrossfadeElapsed = 0.0f;
	float CrossfadeAlpha = 1.0f;
	float ActiveCrossfadeDuration = 0.15f;
	float LandToLocomotionCrossfade = 0.15f;

	float JumpStartPlayRate = 1.0f;
	float FallLoopPlayRate = 1.0f;
	float LandPlayRate = 1.0f;

	/** The standing and crouched locomotion spaces (the base class plays one of them), and the other's samples. */
	UPROPERTY(Transient)
	const UBlendSpaceBase* StandBlendSpace = nullptr;

	UPROPERTY(Transient)
	const UBlendSpaceBase* CrouchBlendSpace = nullptr;

	FBlendSampleDataArray FadingSamples;
	bool bCrouched = false;
	/** The crouched space is the one playing (bCrouched with a crouch space). */
	bool bCrouchActive = false;
	/** The space the last update set: a different one is the owner's new standing space. */
	const UBlendSpaceBase* AppliedBlendSpace = nullptr;
	bool bAppliedBlendSpace = false;
	float CrouchAlpha = 0.0f;

	bool bFalling = false;
	float VelocityZ = 0.0f;
	bool bJustLanded = false;
	bool bJumpRequested = false;
};
