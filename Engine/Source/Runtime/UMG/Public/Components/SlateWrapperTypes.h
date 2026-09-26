#pragma once

#include "CoreTypes.h"

/** Whether a widget is drawn and takes space (UE: ESlateVisibility, Components/SlateWrapperTypes.h). */
enum class ESlateVisibility : uint8
{
	/** Drawn, takes space. */
	Visible,
	/** Not drawn, takes no space. */
	Collapsed,
	/** Not drawn, still takes space. */
	Hidden,
	/** Drawn, takes space; UE's hit-test variants (Leon's widgets take no input). */
	HitTestInvisible,
	SelfHitTestInvisible,
};
