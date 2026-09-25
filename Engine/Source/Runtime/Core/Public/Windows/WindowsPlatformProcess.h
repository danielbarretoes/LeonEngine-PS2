#pragma once

#include "GenericPlatform/GenericPlatformProcess.h"

struct CORE_API FWindowsPlatformProcess : public FGenericPlatformProcess
{
	static const TCHAR* BaseDir();
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static FString GetCurrentWorkingDirectory();
	/** Blocks the calling thread for Seconds (UE: Sleep). */
	static void Sleep(float Seconds);
};

typedef FWindowsPlatformProcess FPlatformProcess;
