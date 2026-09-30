#pragma once

#include "CoreTypes.h"

/** Where a widget sits across the width its slot gives it (UE: EHorizontalAlignment, Types/SlateEnums.h). */
enum EHorizontalAlignment : uint8
{
	HAlign_Fill,
	HAlign_Left,
	HAlign_Center,
	HAlign_Right,
};

/** Where a widget sits across the height its slot gives it (UE: EVerticalAlignment). */
enum EVerticalAlignment : uint8
{
	VAlign_Fill,
	VAlign_Top,
	VAlign_Center,
	VAlign_Bottom,
};

/** A direction focus moves in (UE: EUINavigation). */
enum class EUINavigation : uint8
{
	Left,
	Right,
	Up,
	Down,
	Next,
	Previous,
	Num,
	Invalid,
};

/** What a key asks the focused widget to do besides moving (UE: EUINavigationAction). */
enum class EUINavigationAction : uint8
{
	Accept,
	Back,
	Num,
	Invalid,
};
