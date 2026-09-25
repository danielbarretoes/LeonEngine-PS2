#pragma once

#include "GenericPlatform/GenericPlatformProcess.h"

/**
 * The EE only knows the executable from argv[0] ("host:ThirdPerson.elf" in PCSX2, "cdrom0:\SLUS_000.00;1" on disc),
 * so the base directory is everything up to the last '/', '\' or ':' of it.
 */
struct CORE_API FPS2PlatformProcess : public FGenericPlatformProcess
{
	static const TCHAR* BaseDir();
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static FString GetCurrentWorkingDirectory();
	static void SetArgV0(const TCHAR* ArgV0);
};

typedef FPS2PlatformProcess FPlatformProcess;
