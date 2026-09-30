#pragma once

#include "Components/PanelSlot.h"
#include "Components/SlateWrapperTypes.h"
#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Types/SlateEnums.h"
#include "HorizontalBoxSlot.generated.h"

/**
 * A child's place in a horizontal box (UE: UHorizontalBoxSlot): its padding, its width rule (its desired width, or a
 * share of what the automatic ones leave) and its alignment across the box's height.
 */
UCLASS()
class UMG_API UHorizontalBoxSlot : public UPanelSlot
{
	GENERATED_BODY()

public:
	UHorizontalBoxSlot(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: SetPadding. */
	void SetPadding(const FMargin& InPadding)
	{
		Padding = InPadding;
	}
	[[nodiscard]] const FMargin& GetPadding() const
	{
		return Padding;
	}
	/** UE: SetSize (Automatic: the child's desired width; Fill: its Value's share of the width left). */
	void SetSize(const FSlateChildSize& InSize)
	{
		Size = InSize;
	}
	[[nodiscard]] const FSlateChildSize& GetSize() const
	{
		return Size;
	}
	/** Where the child sits in its width when it is not Fill (UE: SetHorizontalAlignment). */
	void SetHorizontalAlignment(EHorizontalAlignment InAlignment)
	{
		HorizontalAlignment = InAlignment;
	}
	[[nodiscard]] EHorizontalAlignment GetHorizontalAlignment() const
	{
		return HorizontalAlignment;
	}
	/** Where the child sits across the box's height (UE: SetVerticalAlignment). */
	void SetVerticalAlignment(EVerticalAlignment InAlignment)
	{
		VerticalAlignment = InAlignment;
	}
	[[nodiscard]] EVerticalAlignment GetVerticalAlignment() const
	{
		return VerticalAlignment;
	}

private:
	FMargin Padding;
	FSlateChildSize Size = FSlateChildSize(ESlateSizeRule::Automatic);
	EHorizontalAlignment HorizontalAlignment = HAlign_Fill;
	EVerticalAlignment VerticalAlignment = VAlign_Fill;
};
