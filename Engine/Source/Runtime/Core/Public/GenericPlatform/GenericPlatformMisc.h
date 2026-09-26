#pragma once

#include "HAL/Platform.h"

struct FGuid;

/** Low-level platform services (UE: FGenericPlatformMisc, reduced). */
struct CORE_API FGenericPlatformMisc
{
	/**
	 * Writes to the platform debug channel (Windows: debugger output; PS2: EE console). Does not allocate and
	 * works before GLog exists.
	 */
	static void LowLevelOutputDebugString(const TCHAR* Message);

	/** printf-style LowLevelOutputDebugString; formats into a stack buffer and truncates long messages. */
	static void LowLevelOutputDebugStringf(const TCHAR* Format, ...) LEON_PRINTF_FORMAT(1, 2);

	/** Writes to the platform debug channel (UE: LocalPrint). */
	static void LocalPrint(const TCHAR* Message);

	/** True when a debugger is attached (PLATFORM_BREAK is only used then). */
	static bool IsDebuggerPresent()
	{
		return false;
	}

	/**
	 * bForce: terminate the process right away (after a fatal error); otherwise ask the engine loop to stop
	 * at the end of the frame (UE: RequestExit).
	 */
	static void RequestExit(bool bForce);

	/**
	 * RequestExit with the process return code: the forced exit's, or the one the engine loop returns after the
	 * frame (GetRequestedEngineExitCode).
	 */
	static void RequestExitWithStatus(bool bForce, uint8 ReturnCode);

	/**
	 * A new GUID from the calendar time, a counter and FMath::Rand; unique within a process, likely unique across
	 * machines (UE: CreateGuid). Windows uses CoCreateGuid.
	 */
	static void CreateGuid(FGuid& Result);
};
