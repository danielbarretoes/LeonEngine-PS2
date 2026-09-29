#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/IInputInterface.h"
#include "UObject/Object.h"
#include "ForceFeedbackEffect.generated.h"

class UForceFeedbackEffect;

/**
 * One curve of a force feedback effect and the motors it drives (UE: FForceFeedbackChannelDetails). Leon's curve is a
 * line from StartIntensity at the effect's start to EndIntensity at its Duration (UE's is an FRuntimeFloatCurve;
 * Docs/PLANS/ps2-shipping.md N24).
 */
USTRUCT()
struct ENGINE_API FForceFeedbackChannelDetails
{
	GENERATED_BODY()

	UPROPERTY()
	bool bAffectsLeftLarge = true;
	UPROPERTY()
	bool bAffectsLeftSmall = true;
	UPROPERTY()
	bool bAffectsRightLarge = true;
	UPROPERTY()
	bool bAffectsRightSmall = true;

	/** The strength at the start, 0 to 1. */
	UPROPERTY()
	float StartIntensity = 1.0f;
	/** The strength at the effect's end, 0 to 1. */
	UPROPERTY()
	float EndIntensity = 0.0f;
};

/** How an effect plays (UE: FForceFeedbackParameters). */
USTRUCT()
struct ENGINE_API FForceFeedbackParameters
{
	GENERATED_BODY()

	/** An effect played with a tag replaces the one playing with the same tag (UE). */
	UPROPERTY()
	FName Tag;
	/** Plays until stopped (UE). */
	UPROPERTY()
	bool bLooping = false;
	/** Plays while the game is paused (UE). */
	UPROPERTY()
	bool bPlayWhilePaused = false;
};

/**
 * A force feedback effect (UE: UForceFeedbackEffect): the curves of the controller's motors over Duration seconds. The
 * player controller plays it (APlayerController::ClientPlayForceFeedback) and sends the sum of what plays to its
 * controller (IInputInterface::SetForceFeedbackChannelValues): the DualShock's two motors on the PS2.
 */
UCLASS()
class ENGINE_API UForceFeedbackEffect : public UObject
{
	GENERATED_BODY()

public:
	UForceFeedbackEffect(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The curves (UE). */
	UPROPERTY()
	TArray<FForceFeedbackChannelDetails> ChannelDetails;

	/** How long it lasts, seconds (UE computes it from the curves). */
	UPROPERTY()
	float Duration = 0.2f;

	/** UE: GetDuration. */
	[[nodiscard]] float GetDuration() const
	{
		return Duration;
	}

	/**
	 * Adds the curves' strengths at EvalTime (seconds from the start), times ValueMultiplier, to the channels they
	 * drive, keeping the stronger of the two on each channel (UE: GetValues).
	 */
	void GetValues(float EvalTime, FForceFeedbackValues& Values, float ValueMultiplier = 1.0f) const;
};

/** An effect a player controller plays (UE: FActiveForceFeedbackEffect). */
USTRUCT()
struct ENGINE_API FActiveForceFeedbackEffect
{
	GENERATED_BODY()

	UPROPERTY()
	UForceFeedbackEffect* ForceFeedbackEffect = nullptr;

	UPROPERTY()
	FForceFeedbackParameters Parameters;

	/** Seconds since it started. */
	UPROPERTY()
	float PlayTime = 0.0f;

	/**
	 * Advances it by DeltaTime and adds its strengths to Values; false once it is over (a looping one starts again)
	 * (UE: Update).
	 */
	bool Update(float DeltaTime, FForceFeedbackValues& Values);
};
