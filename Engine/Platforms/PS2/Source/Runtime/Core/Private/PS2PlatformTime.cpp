#include "HAL/PlatformTime.h"

#include <timer.h>

namespace
{
	constexpr uint64 BusClockHz = 147456000ull;

	/** PCCR (EE Core User's Manual 7.1.1): what PCR0 and PCR1 count, and in which modes. */
	constexpr uint32 PccrCounterEnable = 1u << 31;
	constexpr uint32 PccrEvent1Shift = 15;
	constexpr uint32 PccrEvent0Shift = 5;
	/** The user, supervisor, kernel and level 1 exception handler bits of PCR1 (14..11) and of PCR0 (4..1). */
	constexpr uint32 PccrAllModes1 = 0xfu << 11;
	constexpr uint32 PccrAllModes0 = 0xfu << 1;
	/** Event 6: an instruction cache miss on PCR0, a data cache miss on PCR1 (Table 7-1). */
	constexpr uint32 EventCacheMiss = 6;
} // namespace

void FPS2PlatformTime::EnablePerfCounters(bool bEnable)
{
	// Stopped while the counters are zeroed, then started with both events (7.2.6 sets PCCR last).
	__asm__ volatile("mtps $0, 0\n\tsync.p");
	ResetPerfCounters();
	if (bEnable)
	{
		const uint32 Pccr = PccrCounterEnable | (EventCacheMiss << PccrEvent1Shift) | PccrAllModes1 |
			(EventCacheMiss << PccrEvent0Shift) | PccrAllModes0;
		__asm__ volatile("mtps %0, 0\n\tsync.p" : : "r"(Pccr));
	}
}

uint64 FPS2PlatformTime::Cycles64()
{
	return GetTimerSystemTime();
}

double FPS2PlatformTime::GetSecondsPerCycle64()
{
	return 1.0 / static_cast<double>(BusClockHz);
}

uint64 FPS2PlatformTime::CyclesToMicroseconds(uint64 Cycles)
{
	// 1e6 / 147456000 = 125 / 18432 (exact, integer only).
	return (Cycles * 125ull) / 18432ull;
}

double FPS2PlatformTime::Seconds()
{
	return static_cast<double>(Cycles64()) * GetSecondsPerCycle64();
}

void FPS2PlatformTime::SystemTime(
	int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec)
{
	// No calendar clock is read yet (the CDVD RTC needs the IOP libcdvd module): a fixed 2000-01-01 00:00:00,
	// a Saturday. Only log names and GUID seeds use it, and the PS2 writes neither.
	Year = 2000;
	Month = 1;
	DayOfWeek = 6;
	Day = 1;
	Hour = 0;
	Min = 0;
	Sec = 0;
	MSec = 0;
}

void FPS2PlatformTime::UtcTime(
	int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec)
{
	SystemTime(Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
}
