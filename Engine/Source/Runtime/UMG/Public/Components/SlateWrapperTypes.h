#pragma once

#include "CoreTypes.h"

/** Whether a widget is drawn and takes space (UE: ESlateVisibility, Components/SlateWrapperTypes.h). */
enum class ESlateVisibility : uint8
{
	/** Drawn, takes space, takes clicks. */
	Visible,
	/** Not drawn, takes no space. */
	Collapsed,
	/** Not drawn, still takes space. */
	Hidden,
	/** Drawn, takes space, takes no clicks (nor do its children). */
	HitTestInvisible,
	/** Drawn, takes space; takes no clicks itself (its children may). */
	SelfHitTestInvisible,
};

/** How a box's slot takes its length (UE: ESlateSizeRule). */
enum class ESlateSizeRule : uint8
{
	/** The child's desired length. */
	Automatic,
	/** A share of the length the automatic slots leave, by Value. */
	Fill,
};

/** A box slot's length rule (UE: FSlateChildSize). */
struct FSlateChildSize
{
	/** The Fill share (UE: Value). */
	float Value = 1.0f;
	/** UE: SizeRule. */
	ESlateSizeRule SizeRule = ESlateSizeRule::Fill;

	FSlateChildSize() = default;
	explicit FSlateChildSize(ESlateSizeRule InSizeRule)
		: SizeRule(InSizeRule)
	{
	}
};
