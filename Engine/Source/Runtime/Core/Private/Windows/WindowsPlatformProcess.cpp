#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformProcess.h"

#include <Windows.h>

namespace
{
	/** Full path of the executable with '/' separators, read once. */
	const FString& GetExecutablePath()
	{
		static const FString Path = []
		{
			TArray<WIDECHAR> Buffer;
			Buffer.SetNumZeroed(MAX_PATH);
			for (;;)
			{
				const DWORD Length = GetModuleFileNameW(nullptr, Buffer.GetData(), DWORD(Buffer.Num()));
				if (Length == 0)
				{
					return FString();
				}
				if (Length < DWORD(Buffer.Num()))
				{
					break;
				}
				Buffer.SetNumZeroed(Buffer.Num() * 2);
			}
			FString Result(Buffer.GetData());
			Result.ReplaceCharInline('\\', '/');
			return Result;
		}();
		return Path;
	}
} // namespace

const TCHAR* FWindowsPlatformProcess::BaseDir()
{
	static const FString Dir = []
	{
		const FString& Path = GetExecutablePath();
		int32 Slash = INDEX_NONE;
		return Path.FindLastChar('/', Slash) ? Path.Mid(0, Slash + 1) : FString();
	}();
	return *Dir;
}

const TCHAR* FWindowsPlatformProcess::ExecutableName(bool bRemoveExtension)
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

FString FWindowsPlatformProcess::GetCurrentWorkingDirectory()
{
	TArray<WIDECHAR> Buffer;
	Buffer.SetNumZeroed(int32(GetCurrentDirectoryW(0, nullptr)) + 1);
	GetCurrentDirectoryW(DWORD(Buffer.Num()), Buffer.GetData());
	FString Result(Buffer.GetData());
	Result.ReplaceCharInline('\\', '/');
	return Result;
}

void FWindowsPlatformProcess::Sleep(float Seconds)
{
	::Sleep(static_cast<DWORD>(Seconds * 1000.0f));
}
