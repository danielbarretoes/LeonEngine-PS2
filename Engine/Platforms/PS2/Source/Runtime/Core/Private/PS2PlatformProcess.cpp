#include "Containers/UnrealString.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CString.h"

namespace
{
	// Fixed buffers: BaseDir is asked for before the heap-backed Core types are worth using, and argv[0] is short.
	TCHAR GBaseDir[256] = "";
	TCHAR GExecutableName[64] = "";
	TCHAR GExecutableNameNoExtension[64] = "";
} // namespace

void FPS2PlatformProcess::SetArgV0(const TCHAR* ArgV0)
{
	if (ArgV0 == nullptr)
	{
		return;
	}

	// The directory ends at the last separator; the device prefix ("host:", "cdrom0:") counts as one.
	const TCHAR* NameStart = ArgV0;
	for (const TCHAR* Char = ArgV0; *Char; ++Char)
	{
		if (*Char == '/' || *Char == '\\' || *Char == ':')
		{
			NameStart = Char + 1;
		}
	}

	const int32 DirLength = int32(NameStart - ArgV0);
	if (DirLength < int32(sizeof(GBaseDir)))
	{
		FCString::Strncpy(GBaseDir, ArgV0, SIZE_T(DirLength + 1));
	}
	else
	{
		// Said on the EE console: every path would otherwise resolve from "" without a word.
		FPlatformMisc::LowLevelOutputDebugString(
			"FPS2PlatformProcess: the executable's folder is too long for the base dir"
			" (255 characters at most); paths resolve from the current folder\n");
	}
	FCString::Strncpy(GExecutableName, NameStart, sizeof(GExecutableName));

	// Disc names carry a ";1" version suffix; the extension ends before it.
	FCString::Strncpy(GExecutableNameNoExtension, NameStart, sizeof(GExecutableNameNoExtension));
	if (TCHAR* Version = FCString::Strchr(GExecutableNameNoExtension, ';'))
	{
		*Version = 0;
	}
	if (TCHAR* Dot = FCString::Strrchr(GExecutableNameNoExtension, '.'))
	{
		*Dot = 0;
	}
}

const TCHAR* FPS2PlatformProcess::BaseDir()
{
	return GBaseDir;
}

const TCHAR* FPS2PlatformProcess::ExecutableName(bool bRemoveExtension)
{
	return bRemoveExtension ? GExecutableNameNoExtension : GExecutableName;
}

FString FPS2PlatformProcess::GetCurrentWorkingDirectory()
{
	// There is no working directory on the EE: paths are relative to the executable's device folder.
	FString Result(GBaseDir);
	if (Result.EndsWith("/") || Result.EndsWith("\\"))
	{
		Result.LeftChopInline(1);
	}
	return Result;
}
