#include "HAL/PlatformTime.h"

#include <Windows.h>

namespace
{
	uint64 QueryFrequency()
	{
		LARGE_INTEGER Frequency;
		QueryPerformanceFrequency(&Frequency);
		return static_cast<uint64>(Frequency.QuadPart);
	}

	uint64 CyclesPerSecond()
	{
		static const uint64 Frequency = QueryFrequency();
		return Frequency;
	}
} // namespace

uint64 FWindowsPlatformTime::Cycles64()
{
	LARGE_INTEGER Counter;
	QueryPerformanceCounter(&Counter);
	return static_cast<uint64>(Counter.QuadPart);
}

double FWindowsPlatformTime::GetSecondsPerCycle64()
{
	return 1.0 / static_cast<double>(CyclesPerSecond());
}

uint64 FWindowsPlatformTime::CyclesToMicroseconds(uint64 Cycles)
{
	return (Cycles / CyclesPerSecond()) * 1000000ull + (Cycles % CyclesPerSecond()) * 1000000ull / CyclesPerSecond();
}

double FWindowsPlatformTime::Seconds()
{
	return static_cast<double>(Cycles64()) * GetSecondsPerCycle64();
}

namespace
{
	void FromSystemTime(const SYSTEMTIME& St, int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour,
		int32& Min, int32& Sec, int32& MSec)
	{
		Year = St.wYear;
		Month = St.wMonth;
		DayOfWeek = St.wDayOfWeek;
		Day = St.wDay;
		Hour = St.wHour;
		Min = St.wMinute;
		Sec = St.wSecond;
		MSec = St.wMilliseconds;
	}
} // namespace

void FWindowsPlatformTime::SystemTime(
	int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec)
{
	SYSTEMTIME St;
	GetLocalTime(&St);
	FromSystemTime(St, Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
}

void FWindowsPlatformTime::UtcTime(
	int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec)
{
	SYSTEMTIME St;
	GetSystemTime(&St);
	FromSystemTime(St, Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
}
