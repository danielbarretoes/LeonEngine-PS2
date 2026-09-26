#pragma once

#include "CoreMinimal.h"

/**
 * The DualShock's analog sticks as the game reads them (Leon; Docs/PLANS/ps2-preview.md V1): libpad reports each axis
 * as a byte (0..255, 128 at rest), and the input interface turns it into -1..1 with a dead zone rescaled to the full
 * range. The PS2 reads the pad's bytes; the desktop turns its gamepad's axes into those bytes first, so a stick moves
 * the game the same on both.
 */
struct FDualShockAnalog
{
	/** The stick travel read as rest, either way. */
	static constexpr float DeadZone = 0.18f;

	/** An axis of -1..1 as libpad's byte. */
	[[nodiscard]] static uint8 ToByte(float Value)
	{
		const float Clamped = FMath::Clamp(Value, -1.0f, 1.0f);
		return uint8(FMath::Clamp(FMath::RoundToInt((Clamped + 1.0f) * 127.5f), 0, 255));
	}

	/** libpad's byte as the game's -1..1, 0 inside the dead zone. */
	[[nodiscard]] static float FromByte(uint8 Raw)
	{
		const float Value = FMath::Clamp((float(Raw) - 128.0f) / 128.0f, -1.0f, 1.0f);
		if (Value > -DeadZone && Value < DeadZone)
		{
			return 0.0f;
		}
		const float Sign = Value < 0.0f ? -1.0f : 1.0f;
		return Sign * ((FMath::Abs(Value) - DeadZone) / (1.0f - DeadZone));
	}

	/** A desktop gamepad's axis as the DualShock would report it. */
	[[nodiscard]] static float FromAxis(float Value)
	{
		return FromByte(ToByte(Value));
	}
};
