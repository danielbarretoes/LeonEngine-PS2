#pragma once

#include "Animation/BlendSpaceBase.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "UObject/Object.h"
#include "AnimInstance.generated.h"

class AActor;
class UAimOffsetBlendSpace1D;
class UAnimMontage;
class UAnimSequence;
class UAnimSequenceBase;
class USkeletalMeshComponent;
class USkeleton;
struct FAnimNotifyEvent;

/** A montage ended: its weight reached 0 (UE: FOnMontageEndedMCDelegate); bInterrupted when stopped or replaced. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMontageEndedMCDelegate, UAnimMontage*, bool);
/** A montage started to blend out (UE: FOnMontageBlendingOutStartedMCDelegate). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMontageBlendingOutStartedMCDelegate, UAnimMontage*, bool);
/**
 * A notify fired (Leon; UE calls the anim blueprint's AnimNotify_<Name> event): its name and the animation it is on.
 * The owning actor binds it (AShooterCharacter: footsteps, the weapon's magazine).
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAnimNotifyMCDelegate, FName, const UAnimSequenceBase*);

/** A montage playing on a slot (UE: FAnimMontageInstance, trimmed). UAnimInstance owns and advances them. */
USTRUCT()
struct ENGINE_API FAnimMontageInstance
{
	GENERATED_BODY()

	UPROPERTY()
	UAnimMontage* Montage = nullptr;

	/** Seconds into the montage (UE: Position). */
	float Position = 0.0f;
	/** UE: PlayRate. */
	float PlayRate = 1.0f;
	/** The slot's weight now, 0 to 1 (UE: GetWeight). */
	float Weight = 0.0f;
	/** The weight the blend goes to: 1 blending in or playing, 0 blending out. */
	float TargetWeight = 1.0f;
	/** The weight the current blend started from, its length and the time spent in it (UE: FAlphaBlend). */
	float BlendStartWeight = 0.0f;
	float BlendTime = 0.0f;
	float BlendElapsed = 0.0f;
	/** The position advances (false once it reached the end: the pose holds the last frame while blending out). */
	bool bPlaying = true;
	/** Stopped or replaced before its end: it fires no more notifies and ends interrupted. */
	bool bInterrupted = false;
	/** A section link set with Montage_SetNextSection: after section From comes To (INDEX_NONE: the end). */
	int32 NextSectionOverrideFrom = INDEX_NONE;
	int32 NextSectionOverrideTo = INDEX_NONE;

	/** Blending out (UE: IsStopped), or ended. */
	[[nodiscard]] bool IsStopped() const
	{
		return TargetWeight <= 0.0f;
	}
};

/** A notify crossed this update, fired after it (UE: FAnimNotifyQueue's entries). */
struct FQueuedAnimNotify
{
	const FAnimNotifyEvent* Event = nullptr;
	const UAnimSequenceBase* Animation = nullptr;
};

/**
 * The animation of a skeletal mesh component (UE: UAnimInstance, its native part): Leon's anim graph is C++, the same
 * for every character (subclasses add states: UCharacterAnimInstance's jump):
 *
 * 1. **Base pose**: a blend space (UBlendSpace1D by speed, UBlendSpace by speed and direction) played synchronized
 *    (EvaluateBasePose; a subclass may put a state machine over it).
 * 2. **Slots**: the montages (Montage_Play) over it: `DefaultSlot` over the whole body, `UpperBody` from the branch
 * bone up (SetUpperBodyBranchBone, a layered blend per bone: the legs keep the locomotion).
 * 3. **Aim offset**: an additive of an UAimOffsetBlendSpace1D at the aim pitch, on the aim branch (the upper body's by
 *    default).
 *
 * A pose is one local transform per bone of the skeleton, blended in local space (FAnimationRuntime); the skeletal mesh
 * component turns the final pose into component-space matrices once (USkeletalMeshComponent::RefreshBoneTransforms).
 * Every temporary pose lives on the frame's stack (FMemStack): an update and an evaluation allocate nothing.
 *
 * **Notifies**: the players queue the notifies they cross (FAnimNotifyEvent; the blend space's highest weighted sample,
 * each montage not interrupted, a subclass's active state), each exactly once per crossing, including a loop's wrap
 * and several in one update; UpdateAnimation fires them in order when the update is done (UAnimNotify::Notify, then
 * OnAnimNotify).
 *
 * It references its skeleton, blend spaces, montages and the clips it samples through UPROPERTYs, so the garbage
 * collector keeps the assets it plays alive.
 */
UCLASS(Transient)
class ENGINE_API UAnimInstance : public UObject
{
	GENERATED_BODY()

public:
	UAnimInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The whole-body slot and the upper body's (UE: slot names of the anim graph's Slot nodes). */
	static const FName DefaultSlotName;
	static const FName UpperBodySlotName;

	void SetOwningMeshComponent(USkeletalMeshComponent* Owner)
	{
		OwningMesh = Owner;
	}
	[[nodiscard]] USkeletalMeshComponent* GetOwningMeshComponent() const
	{
		return OwningMesh;
	}
	/** The owning component's actor (UE: GetOwningActor); null without one. */
	[[nodiscard]] AActor* GetOwningActor() const;

	/** The skeleton the poses are of; the branches' bone weights are rebuilt for it. */
	void SetSkeleton(const USkeleton* InSkeleton);

	// Locomotion (the base pose)

	/** The locomotion blend space (1D or 2D); null for the reference pose. */
	void SetBlendSpace(const UBlendSpaceBase* InBlendSpace)
	{
		BlendSpace = InBlendSpace;
	}
	/** The blend space's input on its first axis (the speed); the second keeps its value. */
	void SetBlendSpaceInput(float AxisValue)
	{
		BlendInputTarget.X = AxisValue;
	}
	/** The input on both axes (X speed, Y direction for a 2D locomotion space). */
	void SetBlendSpaceInput(const FVector& InBlendInput)
	{
		BlendInputTarget = InBlendInput;
	}
	/** The eased input the last update used, and the one it goes to. */
	[[nodiscard]] const FVector& GetBlendSpaceInput() const
	{
		return BlendInput;
	}
	[[nodiscard]] const FVector& GetBlendSpaceInputTarget() const
	{
		return BlendInputTarget;
	}

	/** How fast the input follows its target (1/s, exponential); 0 snaps. */
	void SetLocomotionBlendInterpSpeed(float Speed)
	{
		LocomotionBlendInterpSpeed = Speed >= 0.0f ? Speed : 0.0f;
	}
	[[nodiscard]] float GetLocomotionBlendInterpSpeed() const
	{
		return LocomotionBlendInterpSpeed;
	}

	/** The samples the last update blends and their weights (FBlendSampleData: indices into the blend space's). */
	[[nodiscard]] const FBlendSampleDataArray& GetLocomotionSamples() const
	{
		return LocomotionSamples;
	}
	/** The blend space's synchronized time, 0 to 1 (each sample at this fraction of its length). */
	[[nodiscard]] float GetLocomotionNormalizedTime() const
	{
		return LocomotionNormalizedTime;
	}

	// Layers

	/** The bone the upper body starts at (a spine bone): the UpperBody slot and the aim offset blend from it up. */
	void SetUpperBodyBranchBone(FName BoneName);
	[[nodiscard]] FName GetUpperBodyBranchBone() const
	{
		return UpperBodyBranchBone;
	}
	/** The weight of each bone in the upper body's layer (1 from the branch bone up); empty without a branch. */
	[[nodiscard]] const TArray<float>& GetUpperBodyBoneWeights() const
	{
		return UpperBodyBoneWeights;
	}

	/** The aim offset (an additive by pitch); null for none. */
	void SetAimOffset(const UAimOffsetBlendSpace1D* InAimOffset)
	{
		AimOffset = InAimOffset;
	}
	/** The aim pitch, degrees (-90 down to 90 up). */
	void SetAimOffsetPitch(float Pitch)
	{
		AimPitch = Pitch;
	}
	[[nodiscard]] float GetAimOffsetPitch() const
	{
		return AimPitch;
	}

	// Montages (UE: Montage_*)

	/**
	 * Plays Montage on its slot from InTimeToStartMontageAt at InPlayRate (UE: Montage_Play): a montage on the same
	 * slot blends out, interrupted. Returns the montage's length, 0 when it cannot play (null, no clip, rate 0).
	 */
	float Montage_Play(UAnimMontage* Montage, float InPlayRate = 1.0f, float InTimeToStartMontageAt = 0.0f);
	/** Blends Montage (every montage when null) out over InBlendOutTime, interrupted (UE: Montage_Stop). */
	void Montage_Stop(float InBlendOutTime, const UAnimMontage* Montage = nullptr);
	/** Montage plays and is not blending out; any montage when null (UE: Montage_IsPlaying). */
	[[nodiscard]] bool Montage_IsPlaying(const UAnimMontage* Montage) const;
	/** Montage has an instance, blending out or not (UE: Montage_IsActive). */
	[[nodiscard]] bool Montage_IsActive(const UAnimMontage* Montage) const;
	/** Moves Montage (the active one when null) to a section's start (UE: Montage_JumpToSection). */
	void Montage_JumpToSection(FName SectionName, const UAnimMontage* Montage = nullptr);
	/** After SectionNameToChange, play NextSection instead of the montage's link (UE: Montage_SetNextSection). */
	void Montage_SetNextSection(FName SectionNameToChange, FName NextSection, const UAnimMontage* Montage = nullptr);
	/** Seconds into Montage, 0 when it is not active (UE: Montage_GetPosition). */
	[[nodiscard]] float Montage_GetPosition(const UAnimMontage* Montage) const;
	/** The montage playing and not blending out, the latest first (UE: GetCurrentActiveMontage). */
	[[nodiscard]] UAnimMontage* GetCurrentActiveMontage() const;
	/** The largest weight of the montages on a slot (UE: GetSlotMontageGlobalWeight). */
	[[nodiscard]] float GetSlotMontageGlobalWeight(FName SlotNodeName) const;
	/** The montages playing or blending out. */
	[[nodiscard]] const TArray<FAnimMontageInstance>& GetMontageInstances() const
	{
		return MontageInstances;
	}

	/** Broadcast when a montage's weight reaches 0 (UE: OnMontageEnded). */
	FOnMontageEndedMCDelegate OnMontageEnded;
	/** Broadcast when a montage starts to blend out (UE: OnMontageBlendingOut). */
	FOnMontageBlendingOutStartedMCDelegate OnMontageBlendingOut;
	/** Broadcast for every notify fired (Leon; see the delegate). */
	FOnAnimNotifyMCDelegate OnAnimNotify;

	// Update and evaluation

	/** UE: NativeInitializeAnimation. */
	virtual void NativeInitializeAnimation()
	{
	}

	/**
	 * The update (UE: UpdateAnimation): NativeUpdateAnimation, the montages' advance, then the notifies crossed fired
	 * in order. The skeletal mesh component calls it every tick.
	 */
	void UpdateAnimation(float DeltaSeconds);

	/** The graph's own update (UE: NativeUpdateAnimation): the locomotion's input, time and notifies. */
	virtual void NativeUpdateAnimation(float DeltaTime);

	/**
	 * The final pose: one local transform per bone of the skeleton (the base pose, the slots, the aim offset); none
	 * without a skeleton. OutPose is resized, reusing its memory; the temporaries live on the frame's stack.
	 */
	void EvaluatePose(TArray<FTransform>& OutPose) const;

	/**
	 * The skin matrices of the current pose (InverseBindPose * component space, FAnimationRuntime), for code without a
	 * component; USkeletalMeshComponent caches its pose instead.
	 */
	void GetSkinMatrices(TArray<FMatrix>& OutSkin) const;

	/** The notifies the last update fired, in order (cleared by the next update). */
	[[nodiscard]] int32 GetNumNotifiesFiredLastUpdate() const
	{
		return NumNotifiesFiredLastUpdate;
	}

protected:
	/**
	 * The graph's base pose into OutPose (one transform per bone), before the slots and the aim offset: the locomotion.
	 * A subclass overrides it (UCharacterAnimInstance's state machine).
	 */
	virtual void EvaluateBasePose(TArrayView<FTransform> OutPose) const;

	/** Eases the locomotion input, picks the blend space's samples and advances their time and notifies. */
	void UpdateLocomotion(float DeltaTime);
	/** The locomotion samples of the last update, blended by their weights (the reference pose without samples). */
	void SampleLocomotionPose(TArrayView<FTransform> OutPose) const;
	/** A blend space's samples (from GetSamplesFromBlendInput) at a normalized time, blended by their weights. */
	void SampleBlendSpacePose(const UBlendSpaceBase* Space, const FBlendSampleDataArray& SampleData,
		float NormalizedTime, TArrayView<FTransform> OutPose) const;
	/** A clip at Time, or the reference pose for a clip without the skeleton's number of tracks. */
	void SampleSequencePose(const UAnimSequence* Sequence, float Time, TArrayView<FTransform> OutPose) const;
	/** The skeleton's reference pose. */
	void GetReferencePose(TArrayView<FTransform> OutPose) const;

	/**
	 * Advances a sequence player's time by DeltaTime (already scaled by its rate, not negative): wrapped for a looping
	 * sequence, held at the end of a one-shot. Queues the notifies it crosses when bQueueNotifies.
	 */
	void AdvanceSequencePlayer(
		const UAnimSequenceBase* Sequence, float& InOutTime, float DeltaTime, bool bQueueNotifies);
	/**
	 * Queues the notifies of Sequence (fired as from Source) crossed from Position forward by DeltaTime on a timeline
	 * of Length: wrapping to 0 at the end when bLoop (as many times as the time covers), else stopping at the end (the
	 * notifies at the end included when it reaches it).
	 */
	void QueueNotifies(const UAnimSequenceBase* Sequence, const UAnimSequenceBase* Source, float Position,
		float DeltaTime, float Length, bool bLoop);

	[[nodiscard]] const USkeleton* GetSkeleton() const
	{
		return Skeleton;
	}
	[[nodiscard]] const UBlendSpaceBase* GetBlendSpace() const
	{
		return BlendSpace;
	}
	/** Bones of the skeleton, 0 without one. */
	[[nodiscard]] int32 GetNumBones() const;

private:
	/** Advances the montages: blends, positions, sections, notifies; removes the ended ones. */
	void UpdateMontages(float DeltaTime);
	/** Starts a montage's blend out over BlendOutTime (0: at once); the caller broadcasts OnMontageBlendingOut. */
	void BeginMontageBlendOut(FAnimMontageInstance& Instance, float BlendOutTime, bool bInterrupted);
	/** The section after SectionIndex in Instance (its override, the link, the next in time), INDEX_NONE at the end. */
	[[nodiscard]] int32 GetNextSection(const FAnimMontageInstance& Instance, int32 SectionIndex) const;
	/** Seconds of montage time from the position to the end, following the sections (a loop: very large). */
	[[nodiscard]] float GetMontageTimeToEnd(const FAnimMontageInstance& Instance) const;
	/** The instance of Montage (the latest active one when null), or null. */
	[[nodiscard]] FAnimMontageInstance* FindMontageInstance(const UAnimMontage* Montage);
	[[nodiscard]] const FAnimMontageInstance* FindMontageInstance(const UAnimMontage* Montage) const;
	/** Fires the queued notifies and empties the queue. */
	void TriggerAnimNotifies();
	/** Rebuilds the branches' bone weights for the skeleton. */
	void RebuildBoneWeights();
	/** The montages and the aim offset over OutPose. */
	void ApplySlotsAndAimOffset(TArrayView<FTransform> InOutPose) const;

	UPROPERTY(Transient)
	USkeletalMeshComponent* OwningMesh = nullptr;

	UPROPERTY(Transient)
	const USkeleton* Skeleton = nullptr;

	UPROPERTY(Transient)
	const UBlendSpaceBase* BlendSpace = nullptr;

	UPROPERTY(Transient)
	const UAimOffsetBlendSpace1D* AimOffset = nullptr;

	UPROPERTY(Transient)
	TArray<FAnimMontageInstance> MontageInstances;

	FVector BlendInput = FVector::ZeroVector;
	FVector BlendInputTarget = FVector::ZeroVector;
	float LocomotionBlendInterpSpeed = 0.0f;
	float LocomotionNormalizedTime = 0.0f;
	FBlendSampleDataArray LocomotionSamples;

	FName UpperBodyBranchBone;
	TArray<float> UpperBodyBoneWeights;
	float AimPitch = 0.0f;

	/** The notifies crossed this update (kept for its capacity). */
	TArray<FQueuedAnimNotify> NotifyQueue;
	int32 NumNotifiesFiredLastUpdate = 0;
};
