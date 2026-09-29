#pragma once

#include "Animation/AnimSequenceBase.h"
#include "CoreMinimal.h"
#include "AnimMontage.generated.h"

class UAnimSequence;

/** A named part of a montage's timeline (UE: FCompositeSection). */
USTRUCT()
struct ENGINE_API FCompositeSection
{
	GENERATED_BODY()

	/** UE: SectionName. */
	UPROPERTY()
	FName SectionName;

	/** Where the section starts, seconds (UE: GetTime); it ends where the next section in time starts. */
	UPROPERTY()
	float StartTime = 0.0f;

	/**
	 * The section that plays after this one (UE: NextSectionName): itself to loop it. None plays on into the section
	 * that follows in time, and the last one ends the montage.
	 */
	UPROPERTY()
	FName NextSectionName;
};

/**
 * A one-shot animation played on a slot over the anim graph (UE: UAnimMontage, a subset): a fire, a reload, drawing a
 * weapon, planting, defusing, throwing. Leon's montage has one slot and one clip (UE: a slot track of segments), its
 * optional sections, a blend in and out and its own notifies next to the clip's (both fire while it plays).
 *
 * UAnimInstance::Montage_Play plays it: the slot's weight rises over BlendInTime, the clip plays from the start (or a
 * section) at the play rate, and BlendOutTime before its end the weight falls; at 0 the montage ends and
 * UAnimInstance::OnMontageEnded is broadcast. A montage played on a slot that is playing another interrupts it (the old
 * one blends out over the new one's BlendInTime and ends interrupted).
 *
 * Slots: `DefaultSlot` replaces the whole pose; `UpperBody` (UAnimInstance::UpperBodySlotName) replaces the bones from
 * the anim instance's branch bone up (a layered blend per bone), so the legs keep walking.
 */
UCLASS()
class ENGINE_API UAnimMontage : public UAnimSequenceBase
{
	GENERATED_BODY()

public:
	UAnimMontage(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The whole-body slot (UE: FAnimSlotGroup::DefaultSlotName). */
	static const FName DefaultSlotName;

	/** The slot it plays on (UE: SlotAnimTracks[0].SlotName). */
	UPROPERTY()
	FName SlotName;

	/** The clip (UE: the slot track's segment). SetAnimation also sets the montage's length. */
	UPROPERTY()
	UAnimSequence* Animation = nullptr;

	/** Seconds the slot's weight takes to rise (UE: BlendIn). */
	UPROPERTY()
	float BlendInTime = 0.1f;

	/** Seconds it takes to fall, ending at the montage's end (UE: BlendOut). */
	UPROPERTY()
	float BlendOutTime = 0.15f;

	/** UE: CompositeSections, sorted by StartTime (AddSection keeps them so). */
	UPROPERTY()
	TArray<FCompositeSection> CompositeSections;

	/** Takes InAnimation as the clip; the montage's length becomes its length and it never loops (Leon). */
	void SetAnimation(UAnimSequence* InAnimation);

	/** Adds a section at StartTime, keeping the time order (UE: AddAnimCompositeSection); its index. */
	int32 AddSection(FName InSectionName, float StartTime, FName InNextSectionName = NAME_None);

	/** UE: GetSectionIndex; INDEX_NONE for an unknown name. */
	[[nodiscard]] int32 GetSectionIndex(FName InSectionName) const;

	/** The section playing at Position (the last one starting at or before it); INDEX_NONE without sections. */
	[[nodiscard]] int32 GetSectionIndexFromPosition(float Position) const;

	/** A section's start and end, seconds (UE: GetSectionStartAndEndTime). */
	void GetSectionStartAndEndTime(int32 SectionIndex, float& OutStartTime, float& OutEndTime) const;
};
