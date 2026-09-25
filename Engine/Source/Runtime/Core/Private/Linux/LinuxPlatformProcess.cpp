#include "Containers/UnrealString.h"
#include "HAL/PlatformProcess.h"

#include <limits.h>
#include <unistd.h>

namespace
{
	/** Full path of the executable, read once from /proc/self/exe. */
	const FString& GetExecutablePath()
	{
		static const FString Path = []
		{
			char Buffer[PATH_MAX + 1] = {};
			const ssize_t Length = readlink("/proc/self/exe", Buffer, PATH_MAX);
			return Length > 0 ? FString(int32(Length), Buffer) : FString();
		}();
		return Path;
	}
} // namespace

const TCHAR* FLinuxPlatformProcess::BaseDir()
{
	static const FString Dir = []
	{
		const FString& Path = GetExecutablePath();
		int32 Slash = INDEX_NONE;
		return Path.FindLastChar('/', Slash) ? Path.Mid(0, Slash + 1) : FString();
	}();
	return *Dir;
}

const TCHAR* FLinuxPlatformProcess::ExecutableName(bool bRemoveExtension)
{
	static const FString Name = []
	{
		const FString& Path = GetExecutablePath();
		int32 Slash = INDEX_NONE;
		return Path.FindLastChar('/', Slash) ? Path.Mid(Slash + 1) : Path;
	}();
	static const FString NameNoExtension = []
	{
		int32 Dot = INDEX_NONE;
		return Name.FindLastChar('.', Dot) ? Name.Mid(0, Dot) : Name;
	}();
	return bRemoveExtension ? *NameNoExtension : *Name;
}

FString FLinuxPlatformProcess::GetCurrentWorkingDirectory()
{
	char Buffer[PATH_MAX + 1] = {};
	return getcwd(Buffer, PATH_MAX) != nullptr ? FString(Buffer) : FString();
}
