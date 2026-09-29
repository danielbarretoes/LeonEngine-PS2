#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * The DualShock 2's pressure-sensitive buttons as analog keys (Leon; Docs/PLANS/ps2-shipping.md N24; UE: FKey analog
 * values, as its trigger axes). In pressure mode (padEnterPressMode) libpad's reply carries a byte a button, 0 released
 * to 255 pressed hard, in the order of padButtonStatus: right, left, up, down, triangle, circle, cross, square, L1, R1,
 * L2, R2. Each becomes an axis in [0, 1]: L2 and R2 are UE's Gamepad_LeftTriggerAxis / Gamepad_RightTriggerAxis, the
 * others Leon's `<button>Axis` keys. A pad without pressure (or a desktop pad's digital button) reads 1 when down, 0
 * when up.
 */
struct FDualShockPressure
{
	/** The pressure bytes of a reply. */
	static constexpr int32 NumButtons = 12;

	/** The axis of pressure byte Index (padButtonStatus's order). */
	[[nodiscard]] static const FKey& GetAxisKey(int32 Index)
	{
		static const FKey* const Keys[NumButtons] = {&EKeys::Gamepad_DPad_RightAxis, &EKeys::Gamepad_DPad_LeftAxis,
			&EKeys::Gamepad_DPad_UpAxis, &EKeys::Gamepad_DPad_DownAxis, &EKeys::Gamepad_FaceButton_TopAxis,
			&EKeys::Gamepad_FaceButton_RightAxis, &EKeys::Gamepad_FaceButton_BottomAxis,
			&EKeys::Gamepad_FaceButton_LeftAxis, &EKeys::Gamepad_LeftShoulderAxis, &EKeys::Gamepad_RightShoulderAxis,
			&EKeys::Gamepad_LeftTriggerAxis, &EKeys::Gamepad_RightTriggerAxis};
		return *Keys[Index];
	}

	/** The button whose pressure the axis carries (Gamepad_FaceButton_Bottom for its axis). */
	[[nodiscard]] static const FKey& GetButtonKey(int32 Index)
	{
		static const FKey* const Keys[NumButtons] = {&EKeys::Gamepad_DPad_Right, &EKeys::Gamepad_DPad_Left,
			&EKeys::Gamepad_DPad_Up, &EKeys::Gamepad_DPad_Down, &EKeys::Gamepad_FaceButton_Top,
			&EKeys::Gamepad_FaceButton_Right, &EKeys::Gamepad_FaceButton_Bottom, &EKeys::Gamepad_FaceButton_Left,
			&EKeys::Gamepad_LeftShoulder, &EKeys::Gamepad_RightShoulder, &EKeys::Gamepad_LeftTrigger,
			&EKeys::Gamepad_RightTrigger};
		return *Keys[Index];
	}

	/** The pressure byte index of an axis key, or INDEX_NONE. */
	[[nodiscard]] static int32 IndexOfAxis(const FKey& Axis)
	{
		for (int32 Index = 0; Index < NumButtons; ++Index)
		{
			if (GetAxisKey(Index) == Axis)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** A pressure byte as the axis' value. */
	[[nodiscard]] static float FromByte(uint8 Pressure)
	{
		return float(Pressure) / 255.0f;
	}
};
