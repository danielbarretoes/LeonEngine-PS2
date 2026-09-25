#pragma once

#include "CoreTypes.h"

/** A coordinate axis (UE: EAxis). */
namespace EAxis
{
	enum Type
	{
		None,
		X,
		Y,
		Z,
	};
} // namespace EAxis

/** A set of axes (UE: EAxisList). */
namespace EAxisList
{
	enum Type
	{
		None = 0,
		X = 1,
		Y = 2,
		Z = 4,

		Screen = 8,
		XY = X | Y,
		XZ = X | Z,
		YZ = Y | Z,
		XYZ = X | Y | Z,
		All = XYZ | Screen,

		// Alias over Axis YZ since it isn't commonly used.
		ZRotation = YZ,
	};
} // namespace EAxisList
