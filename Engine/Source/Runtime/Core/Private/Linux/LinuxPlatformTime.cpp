#include "HAL/PlatformTime.h"

#include <time.h>

uint64 FLinuxPlatformTime::Cycles64()
{
	timespec Now;
	clock_gettime(CLOCK_MONOTONIC, &Now);
	return static_cast<uint64>(Now.tv_sec) * 1000000000ull + static_cast<uint64>(Now.tv_nsec);
}

double FLinuxPlatformTime::GetSecondsPerCycle64()
{
	return 1.0e-9;
}

uint64 FLinuxPlatformTime::CyclesToMicroseconds(uint64 Cycles)
{
	return Cycles / 1000ull;
}

double FLinuxPlatformTime::Seconds()
{
	return static_cast<double>(Cycles64()) * GetSecondsPerCycle64();
}

namespace
{
	void FromTm(const tm& Tm, long Nanoseconds, int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour,
		int32& Min, int32& Sec, int32& MSec)
	{
		Year = Tm.tm_year + 1900;
		Month = Tm.tm_mon + 1;
		DayOfWeek = Tm.tm_wday;
		Day = Tm.tm_mday;
		Hour = Tm.tm_hour;
		Min = Tm.tm_min;
		Sec = Tm.tm_sec;
		MSec = int32(Nanoseconds / 1000000);
	}
} // namespace

void FLinuxPlatformTime::SystemTime(
	int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec)
{
	timespec Now;
	clock_gettime(CLOCK_REALTIME, &Now);
	tm Local;
	localtime_r(&Now.tv_sec, &Local);
	FromTm(Local, Now.tv_nsec, Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
}

void FLinuxPlatformTime::UtcTime(
	int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec)
{
	timespec Now;
	clock_gettime(CLOCK_REALTIME, &Now);
	tm Utc;
	gmtime_r(&Now.tv_sec, &Utc);
	FromTm(Utc, Now.tv_nsec, Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
}
