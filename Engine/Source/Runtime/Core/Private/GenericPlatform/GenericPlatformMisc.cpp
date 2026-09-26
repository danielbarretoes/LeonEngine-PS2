#include "CoreGlobals.h"
#include "HAL/PlatformMath.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

void FGenericPlatformMisc::LowLevelOutputDebugString(const TCHAR* Message)
{
	FPlatformMisc::LocalPrint(Message);
}

void FGenericPlatformMisc::LowLevelOutputDebugStringf(const TCHAR* Format, ...)
{
	TCHAR Buffer[1024];
	va_list Args;
	va_start(Args, Format);
	std::vsnprintf(Buffer, sizeof(Buffer), Format, Args);
	va_end(Args);
	FPlatformMisc::LowLevelOutputDebugString(Buffer);
}

void FGenericPlatformMisc::LocalPrint(const TCHAR* Message)
{
	std::fputs(Message, stdout);
	std::fflush(stdout);
}

void FGenericPlatformMisc::RequestExit(bool bForce)
{
	FPlatformMisc::RequestExitWithStatus(bForce, 3);
}

void FGenericPlatformMisc::RequestExitWithStatus(bool bForce, uint8 ReturnCode)
{
	if (bForce)
	{
		std::fflush(stdout);
		std::fflush(stderr);
		std::_Exit(ReturnCode);
	}
	SetRequestedEngineExitCode(ReturnCode);
	RequestEngineExit("FPlatformMisc::RequestExit");
}

void FGenericPlatformMisc::CreateGuid(FGuid& Result)
{
	static uint16 IncrementCounter = 0;

	// Use real time for baseline uniqueness.
	int32 Year = 0, Month = 0, DayOfWeek = 0, Day = 0, Hour = 0, Min = 0, Sec = 0, MSec = 0;
	FPlatformTime::SystemTime(Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);

	// Sequential bits keep GUIDs made in the same millisecond apart; random bits help across machines.
	const uint32 SequentialBits = uint32(IncrementCounter++);
	const uint32 RandBits = uint32(FPlatformMath::Rand()) & 0xFFFF;

	Result = FGuid(RandBits | (SequentialBits << 16),
		uint32(Day) | (uint32(Hour) << 8) | (uint32(Month) << 16) | (uint32(Sec) << 24),
		uint32(MSec) | (uint32(Min) << 16), uint32(Year) ^ uint32(FPlatformTime::Cycles64()));
}
