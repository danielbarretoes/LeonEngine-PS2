#include "Misc/OutputDeviceRedirector.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformOutputDevices.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/OutputDeviceStdOutput.h"
#include "UObject/NameTypes.h"

#include <cstdio>
#include <new>

void FOutputDeviceRedirector::AddOutputDevice(FOutputDevice* OutputDevice)
{
	if (OutputDevice)
	{
		OutputDevices.AddUnique(OutputDevice);
	}
}

void FOutputDeviceRedirector::RemoveOutputDevice(FOutputDevice* OutputDevice)
{
	OutputDevices.Remove(OutputDevice);
}

bool FOutputDeviceRedirector::IsRedirectingTo(FOutputDevice* OutputDevice) const
{
	return OutputDevices.Contains(OutputDevice);
}

void FOutputDeviceRedirector::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
	Serialize(V, Verbosity, Category, -1.0);
}

void FOutputDeviceRedirector::Serialize(
	const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, const double Time)
{
	if (bIsTornDown)
	{
		return;
	}

	// A device that logs from inside Serialize must not recurse forever.
	if (SerializeDepth > 2)
	{
		return;
	}
	++SerializeDepth;
	// Index loop: a device may add another device while handling the message.
	for (int32 Index = 0; Index < OutputDevices.Num(); ++Index)
	{
		OutputDevices[Index]->Serialize(V, Verbosity, Category, Time);
	}
	--SerializeDepth;
}

void FOutputDeviceRedirector::Flush()
{
	for (FOutputDevice* Device : OutputDevices)
	{
		Device->Flush();
	}
}

void FOutputDeviceRedirector::TearDown()
{
	Flush();
	for (FOutputDevice* Device : OutputDevices)
	{
		Device->TearDown();
	}
	OutputDevices.Empty();
	bIsTornDown = true;
}

FOutputDeviceRedirector* GetGlobalLogSingleton()
{
	// Created on first use and never destroyed (statics may log while the program exits).
	alignas(FOutputDeviceRedirector) static uint8 Storage[sizeof(FOutputDeviceRedirector)];
	static FOutputDeviceRedirector* Singleton = []()
	{
		FOutputDeviceRedirector* Redirector = new (Storage) FOutputDeviceRedirector();
		FPlatformOutputDevices::SetupOutputDevices(*Redirector);
		return Redirector;
	}();
	return Singleton;
}

void FOutputDeviceStdOutput::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
	const FString Line = bSuppressEventTag ? FString(V) : FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V);
	std::fputs(*Line, stdout);
	if (bAutoEmitLineTerminator)
	{
		std::fputc('\n', stdout);
	}
	// Errors must reach the console even if the program stops right after.
	if ((Verbosity & ELogVerbosity::VerbosityMask) <= ELogVerbosity::Error)
	{
		std::fflush(stdout);
	}
}

void FOutputDeviceStdOutput::Flush()
{
	std::fflush(stdout);
}

void FOutputDeviceDebug::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
	FString Line = bSuppressEventTag ? FString(V) : FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V);
	if (bAutoEmitLineTerminator)
	{
		Line += TEXT("\n");
	}
	FPlatformMisc::LowLevelOutputDebugString(*Line);
}

void FGenericPlatformOutputDevices::SetupOutputDevices(FOutputDeviceRedirector& Log)
{
	// The console is the debug channel on the generic platforms (PS2: EE console).
	alignas(FOutputDeviceStdOutput) static uint8 Storage[sizeof(FOutputDeviceStdOutput)];
	static FOutputDeviceStdOutput* StdOut = new (Storage) FOutputDeviceStdOutput();
	Log.AddOutputDevice(StdOut);
}

#if PLATFORM_WINDOWS
void FWindowsPlatformOutputDevices::SetupOutputDevices(FOutputDeviceRedirector& Log)
{
	FGenericPlatformOutputDevices::SetupOutputDevices(Log);

	alignas(FOutputDeviceDebug) static uint8 Storage[sizeof(FOutputDeviceDebug)];
	static FOutputDeviceDebug* Debug = new (Storage) FOutputDeviceDebug();
	Log.AddOutputDevice(Debug);
}
#endif
