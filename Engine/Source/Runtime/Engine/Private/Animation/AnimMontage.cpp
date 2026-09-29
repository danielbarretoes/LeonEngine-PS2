#include "Animation/AnimMontage.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"

const FName UAnimMontage::DefaultSlotName(TEXT("DefaultSlot"));

UAnimMontage::UAnimMontage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	// A literal, not DefaultSlotName: the class default object may be built before that static is.
	, SlotName(TEXT("DefaultSlot"))
{
	bLoop = false;
}

void UAnimMontage::SetAnimation(UAnimSequence* InAnimation)
{
	Animation = InAnimation;
	SequenceLength = InAnimation != nullptr ? InAnimation->GetPlayLength() : 0.0f;
	bLoop = false;
	if (InAnimation != nullptr)
	{
		SetSkeleton(InAnimation->GetSkeleton());
	}
}

int32 UAnimMontage::AddSection(FName InSectionName, float StartTime, FName InNextSectionName)
{
	FCompositeSection Section;
	Section.SectionName = InSectionName;
	Section.StartTime = FMath::Clamp(StartTime, 0.0f, FMath::Max(SequenceLength, 0.0f));
	Section.NextSectionName = InNextSectionName;
	int32 Index = CompositeSections.Num();
	while (Index > 0 && CompositeSections[Index - 1].StartTime > Section.StartTime)
	{
		--Index;
	}
	CompositeSections.Insert(Section, Index);
	return Index;
}

int32 UAnimMontage::GetSectionIndex(FName InSectionName) const
{
	for (int32 Index = 0; Index < CompositeSections.Num(); ++Index)
	{
		if (CompositeSections[Index].SectionName == InSectionName)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UAnimMontage::GetSectionIndexFromPosition(float Position) const
{
	int32 Found = INDEX_NONE;
	for (int32 Index = 0; Index < CompositeSections.Num(); ++Index)
	{
		if (CompositeSections[Index].StartTime <= Position || Found == INDEX_NONE)
		{
			Found = Index;
		}
		if (CompositeSections[Index].StartTime > Position)
		{
			break;
		}
	}
	return Found;
}

void UAnimMontage::GetSectionStartAndEndTime(int32 SectionIndex, float& OutStartTime, float& OutEndTime) const
{
	OutStartTime = 0.0f;
	OutEndTime = GetPlayLength();
	if (!CompositeSections.IsValidIndex(SectionIndex))
	{
		return;
	}
	OutStartTime = CompositeSections[SectionIndex].StartTime;
	if (CompositeSections.IsValidIndex(SectionIndex + 1))
	{
		OutEndTime = CompositeSections[SectionIndex + 1].StartTime;
	}
}
