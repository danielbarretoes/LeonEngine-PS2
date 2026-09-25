#include "Misc/OutputDevice.h"

#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"
#include "Misc/OutputDeviceHelper.h"
#include "UObject/NameTypes.h"

#include <cstdarg>

void FOutputDevice::Log(const TCHAR* S)
{
	Serialize(S, ELogVerbosity::Log, NAME_None);
}

void FOutputDevice::Log(ELogVerbosity::Type Verbosity, const TCHAR* S)
{
	Serialize(S, Verbosity, NAME_None);
}

void FOutputDevice::Log(const FName& Category, ELogVerbosity::Type Verbosity, const TCHAR* Str)
{
	Serialize(Str, Verbosity, Category);
}

void FOutputDevice::Log(const FString& S)
{
	Serialize(*S, ELogVerbosity::Log, NAME_None);
}

void FOutputDevice::Log(const FText& S)
{
	Serialize(*S.ToString(), ELogVerbosity::Log, NAME_None);
}

void FOutputDevice::Log(ELogVerbosity::Type Verbosity, const FString& S)
{
	Serialize(*S, Verbosity, NAME_None);
}

void FOutputDevice::Log(const FName& Category, ELogVerbosity::Type Verbosity, const FString& S)
{
	Serialize(*S, Verbosity, Category);
}

void FOutputDevice::Logf(const TCHAR* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	const FString Message = FString::PrintfImpl(Fmt, Args);
	va_end(Args);
	Serialize(*Message, ELogVerbosity::Log, NAME_None);
}

void FOutputDevice::Logf(ELogVerbosity::Type Verbosity, const TCHAR* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	const FString Message = FString::PrintfImpl(Fmt, Args);
	va_end(Args);
	Serialize(*Message, Verbosity, NAME_None);
}

void FOutputDevice::CategorizedLogf(const FName& Category, ELogVerbosity::Type Verbosity, const TCHAR* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	const FString Message = FString::PrintfImpl(Fmt, Args);
	va_end(Args);
	Serialize(*Message, Verbosity, Category);
}

FString FOutputDeviceHelper::FormatLogLine(ELogVerbosity::Type Verbosity, const FName& Category, const TCHAR* Message)
{
	FString Format;
	const ELogVerbosity::Type Level = ELogVerbosity::Type(Verbosity & ELogVerbosity::VerbosityMask);
	if (!Category.IsNone())
	{
		Category.AppendString(Format);
		Format += TEXT(": ");
	}
	if (Level != ELogVerbosity::Log && Level != ELogVerbosity::NoLogging)
	{
		Format += ToString(Level);
		Format += TEXT(": ");
	}
	if (Message)
	{
		Format += Message;
	}
	return Format;
}
