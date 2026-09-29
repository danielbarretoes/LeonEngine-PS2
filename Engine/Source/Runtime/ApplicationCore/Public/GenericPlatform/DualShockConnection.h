#pragma once

#include "CoreMinimal.h"

/** A command the game sends a DualShock (libpad's), one at a time: the pad answers each through its states. */
enum class EDualShockCommand : uint8
{
	None,
	/** padSetMainMode(DUALSHOCK, LOCK): the analog mode. */
	SetAnalogMode,
	/** padSetActAlign: the small and large motors to the vibration bytes (N24). */
	SetActuatorAlign,
	/** padEnterPressMode: the buttons' pressure in the pad's reply (N24). */
	EnterPressureMode,
};

/**
 * What the game asks a DualShock when (Leon; Docs/PLANS/ps2-shipping.md N5, N24). A pad plugged in, or plugged back
 * after it was pulled out, starts digital, with its motors unaligned and without pressure; libpad's
 * padSetMainMode(DUALSHOCK, LOCK) makes it analog, then padSetActAlign readies the motors and padEnterPressMode sends
 * the pressures. The pad answers each command through a few states (a command in flight, then stable), so one is sent a
 * frame at most, when the pad is readable. The PS2's input interface feeds Update each frame with what libpad reports
 * and sends the command it returns; the logic is platform-neutral so it is tested on every platform.
 *
 * - A pad that disconnects forgets everything, so the next connection asks again.
 * - A readable pad that is not in DualShock mode is asked for it once; if it still is not after RetryFrames readable
 *   frames (the command was lost, or the pad refused it), it is asked again. Leaving the mode forgets the motors and
 * the pressure (the pad resets them).
 * - A pad in DualShock mode gets its motors aligned (when it has them) and then its pressure mode (when it has it),
 * each once.
 */
class FDualShockConnection
{
public:
	/** Readable frames to wait for a requested mode before asking again. */
	static constexpr int32 RetryFrames = 60;

	/**
	 * One frame: bConnected is false when libpad reports no pad (PAD_STATE_DISCONN), bReadable when it can be read
	 * (stable), bDualShockMode when its current mode is the DualShock's; bHasActuators / bHasPressure what the pad says
	 * it has (padInfoAct, padInfoPressMode; read in DualShock mode). Returns the command to send now.
	 */
	[[nodiscard]] EDualShockCommand Update(
		bool bConnected, bool bReadable, bool bDualShockMode, bool bHasActuators = false, bool bHasPressure = false)
	{
		if (!bConnected)
		{
			*this = FDualShockConnection(NumRequests);
			return EDualShockCommand::None;
		}
		if (!bReadable)
		{
			return EDualShockCommand::None;
		}
		if (!bDualShockMode)
		{
			bActuatorsAligned = false;
			bPressureMode = false;
			if (bRequested && ++FramesSinceRequest < RetryFrames)
			{
				return EDualShockCommand::None;
			}
			bRequested = true;
			FramesSinceRequest = 0;
			++NumRequests;
			return EDualShockCommand::SetAnalogMode;
		}
		if (bHasActuators && !bActuatorsAligned)
		{
			bActuatorsAligned = true;
			return EDualShockCommand::SetActuatorAlign;
		}
		if (bHasPressure && !bPressureMode)
		{
			bPressureMode = true;
			return EDualShockCommand::EnterPressureMode;
		}
		return EDualShockCommand::None;
	}

	/** How many times the analog mode was requested (logs, tests). */
	[[nodiscard]] int32 GetNumRequests() const
	{
		return NumRequests;
	}

	/** Whether the motors were aligned since the pad connected (vibration can be sent). */
	[[nodiscard]] bool AreActuatorsAligned() const
	{
		return bActuatorsAligned;
	}

	/** Whether the pressure mode was entered since the pad connected (the pressures are in the reply). */
	[[nodiscard]] bool IsPressureMode() const
	{
		return bPressureMode;
	}

	FDualShockConnection() = default;

private:
	explicit FDualShockConnection(int32 InNumRequests)
		: NumRequests(InNumRequests)
	{
	}

	bool bRequested = false;
	bool bActuatorsAligned = false;
	bool bPressureMode = false;
	int32 FramesSinceRequest = 0;
	int32 NumRequests = 0;
};
