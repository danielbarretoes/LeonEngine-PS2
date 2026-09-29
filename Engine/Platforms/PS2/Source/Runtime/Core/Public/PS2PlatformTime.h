#pragma once

#include "GenericPlatform/GenericPlatformTime.h"

/**
 * The EE's clocks.
 *
 * - Cycles64: GetTimerSystemTime(), the kernel's BUSCLK count (147.456 MHz, a kernel call).
 * - Cycles: the COP0 Count register, the CPU clock (294.912 MHz): one `mfc0`, what the cycle stats read at every
 *   scope. It wraps every 14.6 s, so an interval is the uint32 difference of two readings.
 * - The performance counters (EE Core User's Manual 7): PCCR sets what PCR0 / PCR1 count, here the instruction cache
 *   misses (PCR0, event 6) and the data cache misses (PCR1, event 6) in every mode (user, supervisor, kernel and the
 *   level 1 exception handlers). A counter's bit 31 is its overflow flag, and a set flag raises the level 2 counter
 *   exception (not maskable): the stats zero them every frame, far from the 2^31 events that would set it. The CPU
 *   cycle event is Count's, which needs no counter (and on a PCR would reach bit 31 in 7.3 s, a long load).
 *
 * COP0 is usable in user mode (the kernel starts the ELF with Status.CU0 set: ps2sdk's ee_kmode_enter writes Status).
 */
struct CORE_API FPS2PlatformTime : public FGenericPlatformTime
{
	static uint64 Cycles64();
	static double GetSecondsPerCycle64();
	static uint64 CyclesToMicroseconds(uint64 Cycles);

	static FORCEINLINE uint32 Cycles()
	{
		uint32 Count;
		__asm__ volatile("mfc0 %0, $9" : "=r"(Count));
		return Count;
	}

	static double GetSecondsPerCycle()
	{
		return 1.0 / 294912000.0;
	}

	static constexpr int32 NumPerfCounters = 2;

	static void EnablePerfCounters(bool bEnable);

	static FORCEINLINE void ResetPerfCounters()
	{
		__asm__ volatile("mtpc $0, 0\n\tmtpc $0, 1\n\tsync.p");
	}

	static FORCEINLINE uint32 ReadPerfCounter(int32 Index)
	{
		uint32 Value;
		if (Index == 0)
		{
			__asm__ volatile("mfpc %0, 0" : "=r"(Value));
		}
		else
		{
			__asm__ volatile("mfpc %0, 1" : "=r"(Value));
		}
		return Value;
	}

	static const TCHAR* GetPerfCounterName(int32 Index)
	{
		return Index == 0 ? TEXT("icache_misses") : TEXT("dcache_misses");
	}

	static double Seconds();
	static void SystemTime(
		int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);
	static void UtcTime(
		int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);
};

typedef FPS2PlatformTime FPlatformTime;
