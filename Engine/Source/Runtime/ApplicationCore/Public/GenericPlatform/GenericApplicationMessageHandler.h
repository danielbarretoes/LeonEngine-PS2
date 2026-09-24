#pragma once

#include "CoreTypes.h"

/** Mouse buttons (UE: EMouseButtons in GenericApplicationMessageHandler.h). Values match GLFW. */
enum class EMouseButtons : int32
{
	Left = 0,
	Right = 1,
	Middle = 2,
	Thumb01 = 3,
	Thumb02 = 4,
	Invalid = -1,
};
