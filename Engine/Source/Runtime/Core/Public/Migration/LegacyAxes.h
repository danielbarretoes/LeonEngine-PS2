#pragma once

#include "Math/Vector.h"

/**
 * The world convention the engine uses until P7: Y up, 1 unit = 1 metre, right-handed like glm; at yaw 0 forward is
 * +X and right is +Z. Code that means "up" uses LegacyAxes::Up, never FVector::UpVector (which is UE's Z up), so the
 * P7 switch to Z-up centimetres finds every use. Desktop and PS2; removed in P7.
 */
namespace LegacyAxes
{
	inline constexpr FVector Up(0.0f, 1.0f, 0.0f);
	inline constexpr FVector Forward(1.0f, 0.0f, 0.0f);
	inline constexpr FVector Right(0.0f, 0.0f, 1.0f);

	/** World units per metre. */
	inline constexpr float UnitsPerMetre = 1.0f;
} // namespace LegacyAxes
