#include "Misc/AssertionMacros.h"

#include "HAL/PlatformMisc.h"
#include "Logging/LogMacros.h"
#include "Misc/OutputDeviceRedirector.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace
{
	int32 GNumEnsureFailures = 0;
	bool GIsReportingCheckFailure = false;
	int32 GNestedFailures = 0;

	/** "<Kind>: <Expr> [File:<File>] [Line: <Line>]" plus the optional formatted message on the next line. */
	void FormatFailure(TCHAR (&Buffer)[2048], const ANSICHAR* Kind, const ANSICHAR* Expr, const ANSICHAR* File,
		int32 Line, const TCHAR* Format, va_list* Args)
	{
		int Written = std::snprintf(Buffer, sizeof(Buffer), "%s: %s [File:%s] [Line: %d]", Kind, Expr, File, Line);
		if (Written < 0)
		{
			Buffer[0] = '\0';
			return;
		}
		if (Format && *Format && Written < int(sizeof(Buffer)) - 2)
		{
			Buffer[Written++] = '\n';
			std::vsnprintf(Buffer + Written, sizeof(Buffer) - SIZE_T(Written), Format, *Args);
		}
	}

	void EmitFailure(const TCHAR* Message)
	{
		if (GIsReportingCheckFailure && GNestedFailures++ > 0)
		{
			// Failing again while reporting (e.g. inside a log device): only the raw debug channel is safe.
			FPlatformMisc::LowLevelOutputDebugStringf("%s\n", Message);
			return;
		}
		GLog->Serialize(Message, ELogVerbosity::Error, LogOutputDevice.GetCategoryName());
		GLog->Flush();
	}

	[[noreturn]] void CheckFailedImpl(
		const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format, va_list* Args)
	{
		GIsReportingCheckFailure = true;
		TCHAR Buffer[2048];
		FormatFailure(Buffer, "Assertion failed", Expr, File, Line, Format, Args);
		EmitFailure(Buffer);
		FPlatformMisc::RequestExit(true);
		std::abort();
	}

	void EnsureFailedImpl(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format, va_list* Args)
	{
		++GNumEnsureFailures;
		TCHAR Buffer[2048];
		FormatFailure(Buffer, "Ensure condition failed", Expr, File, Line, Format, Args);
		EmitFailure(Buffer);
	}
} // namespace

void FDebug::CheckFailed(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line)
{
	CheckFailedImpl(Expr, File, Line, nullptr, nullptr);
}

void FDebug::CheckFailedf(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	CheckFailedImpl(Expr, File, Line, Format, &Args);
}

void FDebug::EnsureFailed(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line)
{
	EnsureFailedImpl(Expr, File, Line, nullptr, nullptr);
}

void FDebug::EnsureFailedf(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	EnsureFailedImpl(Expr, File, Line, Format, &Args);
	va_end(Args);
}

int32 FDebug::GetNumEnsureFailures()
{
	return GNumEnsureFailures;
}
