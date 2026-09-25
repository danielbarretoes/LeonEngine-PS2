#pragma once

#include "HAL/Platform.h"

class FString;

/** Process information (UE: FGenericPlatformProcess). Only what the paths and the log need so far. */
struct CORE_API FGenericPlatformProcess
{
	/** Directory of the running executable, with a trailing '/' (UE: BaseDir). Empty when unknown. */
	static const TCHAR* BaseDir();

	/** File name of the running executable (UE: ExecutableName). */
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);

	/** Current working directory with '/' separators, no trailing '/' (UE: GetCurrentWorkingDirectory). */
	static FString GetCurrentWorkingDirectory();

	/**
	 * Leon extension: platforms that only learn the executable path from argv[0] (PS2) record it here. GuardedMain
	 * and the program mains call it before anything asks for BaseDir.
	 */
	static void SetArgV0(const TCHAR* /*ArgV0*/)
	{
	}
};
