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

/** Framework character AnimBP: locomotion UBlendSpace1D + Jump / Fall / Land state machine (rates game-tuned). */
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
	void EnterState(EAnimJumpState Next);
	void UpdateJumpStateMachine();
	void SamplePlayerBoneWorld(const FAnimPosePlayer& Player, TArray<FMatrix>& OutBoneWorld) const;
	void AdvancePlayer(FAnimPosePlayer& Player, float DeltaTime, float PlayRate) const;
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

	bool bFalling = false;
	float VelocityZ = 0.0f;
	bool bJustLanded = false;
	bool bJumpRequested = false;
};
