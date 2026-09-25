#include "CoreGlobals.h"
#include "HAL/PlatformMisc.h"

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
	RequestEngineExit("FPlatformMisc::RequestExit");
}
