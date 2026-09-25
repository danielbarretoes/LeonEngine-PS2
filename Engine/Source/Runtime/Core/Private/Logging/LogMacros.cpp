#include "Logging/LogMacros.h"

#include "Containers/UnrealString.h"
#include "HAL/PlatformMisc.h"
#include "Misc/OutputDeviceRedirector.h"

#include <cstdarg>
#include <cstdlib>

DEFINE_LOG_CATEGORY(LogTemp);
DEFINE_LOG_CATEGORY(LogCore);
DEFINE_LOG_CATEGORY(LogInit);
DEFINE_LOG_CATEGORY(LogExit);
DEFINE_LOG_CATEGORY(LogMemory);
DEFINE_LOG_CATEGORY(LogModuleManager);
DEFINE_LOG_CATEGORY(LogOutputDevice);

void FMsg::Log(
	const ANSICHAR* File, int32 Line, const FName& Category, ELogVerbosity::Type Verbosity, const TCHAR* Message)
{
	if ((Verbosity & ELogVerbosity::VerbosityMask) != ELogVerbosity::Fatal)
	{
		GLog->Serialize(Message, ELogVerbosity::Type(Verbosity & ELogVerbosity::VerbosityMask), Category);
		return;
	}

	// Fatal: log where it happened, flush every device and stop.
	GLog->Serialize(Message, ELogVerbosity::Fatal, Category);
	GLog->Logf(ELogVerbosity::Fatal, "Fatal error: [File:%s] [Line: %d]", File, Line);
	GLog->Flush();
	UE_DEBUG_BREAK();
	FPlatformMisc::RequestExit(true);
	std::abort();
}

void FMsg::Logf(
	const ANSICHAR* File, int32 Line, const FName& Category, ELogVerbosity::Type Verbosity, const TCHAR* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	const FString Message = FString::PrintfImpl(Fmt, Args);
	va_end(Args);
	Log(File, Line, Category, Verbosity, *Message);
}
