#pragma once

#include "Animation/AnimationAsset.h"
#include "CoreMinimal.h"
#include "AnimSequenceBase.generated.h"

class UAnimNotify;

/**
 * An event on an animation's timeline (UE: FAnimNotifyEvent, AnimTypes.h): when a player crosses TriggerTime the anim
 * instance fires it (UAnimInstance's notify queue). A notify with a Notify object calls UAnimNotify::Notify; every
 * notify, named or not, is broadcast to UAnimInstance::OnAnimNotify, which the owning actor binds (UE calls an
 * AnimNotify_<NotifyName> event of the anim blueprint).
 */
USTRUCT()
struct ENGINE_API FAnimNotifyEvent
{
	GENERATED_BODY()

	/** Seconds from the start of the animation (UE: GetTriggerTime). */
	UPROPERTY()
	float TriggerTime = 0.0f;

	/** The notify's name: `Footstep_L`, `MagOut` (UE: NotifyName). */
	UPROPERTY()
	FName NotifyName;

	/** An object that handles the event itself, or null for a named notify (UE: Notify). */
	UPROPERTY()
	UAnimNotify* Notify = nullptr;
};

/** The base of the assets that play over time (UE: UAnimSequenceBase). */
UCLASS(Abstract)
class ENGINE_API UAnimSequenceBase : public UAnimationAsset
{
	GENERATED_BODY()

public:
	UAnimSequenceBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Length in seconds at rate 1 (UE: SequenceLength). */
	UPROPERTY()
	float SequenceLength = 1.0f;

	/** Speed multiplier (UE: RateScale; Leon's anim instances pass their own play rates). */
	UPROPERTY()
	float RateScale = 1.0f;

	/** Wraps around at the end; a one-shot holds its last frame (UE: bLoop). */
	UPROPERTY()
	bool bLoop = true;

	/** The notifies, sorted by TriggerTime (UE: Notifies). Add them with AddNotify. */
	UPROPERTY()
	TArray<FAnimNotifyEvent> Notifies;

	/** UE: GetPlayLength. */
	[[nodiscard]] virtual float GetPlayLength() const
	{
		return SequenceLength;
	}

	/** Sampled frames (UE: GetNumberOfFrames); none in the base. */
	[[nodiscard]] virtual int32 GetNumberOfFrames() const
	{
		return 0;
	}

	/**
	 * Adds a notify at Time (clamped to the length), after those at the same time (Leon; UE's editor adds them).
	 * Returns its index.
	 */
	int32 AddNotify(FName InNotifyName, float Time, UAnimNotify* InNotify = nullptr);

	/**
	 * The notifies a player crosses moving forward from PreviousPosition to CurrentPosition, without wrapping (UE:
	 * GetAnimNotifiesFromDeltaPositions): those with PreviousPosition <= TriggerTime < CurrentPosition, and those at
	 * CurrentPosition too when bIncludeEnd (a one-shot reaching its end). None when the positions are equal. The caller
	 * splits a wrap of a looping player into two calls.
	 */
	template <typename AllocatorType>
	void GetAnimNotifiesFromDeltaPositions(float PreviousPosition, float CurrentPosition, bool bIncludeEnd,
		TArray<const FAnimNotifyEvent*, AllocatorType>& OutNotifies) const
	{
		if (CurrentPosition <= PreviousPosition)
		{
			return;
		}
		for (const FAnimNotifyEvent& Event : Notifies)
		{
			if (Event.TriggerTime < PreviousPosition)
			{
				continue;
			}
			if (Event.TriggerTime < CurrentPosition || (bIncludeEnd && Event.TriggerTime <= CurrentPosition))
			{
				OutNotifies.Add(&Event);
				continue;
			}
			break;
		}
	}
};
