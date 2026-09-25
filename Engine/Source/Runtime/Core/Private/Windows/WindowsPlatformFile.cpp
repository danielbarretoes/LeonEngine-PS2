#include "Containers/StringConv.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/DateTime.h"
#include "Windows/WindowsHWrapper.h"

// Windows.h maps these names to their W / A variants; the IPlatformFile methods keep the UE names.
#undef DeleteFile
#undef MoveFile
#undef CopyFile
#undef CreateDirectory

namespace
{
	/** FILETIME counts 100 ns ticks since 1601-01-01, like FDateTime's ticks from year 1. */
	FDateTime FromFileTime(const FILETIME& FileTime)
	{
		const int64 Ticks = (int64(FileTime.dwHighDateTime) << 32) | int64(FileTime.dwLowDateTime);
		return FDateTime(1601, 1, 1) + FTimespan(Ticks);
	}

	/** Directory path without trailing separators, for the Win32 calls. */
	FString TrimDirectory(const TCHAR* Directory)
	{
		FString Result(Directory);
		Result.ReplaceCharInline('\\', '/');
		while (Result.Len() > 1 && Result[Result.Len() - 1] == '/' && !(Result.Len() == 3 && Result[1] == ':'))
		{
			Result.LeftChopInline(1);
		}
		return Result;
	}

	class FFileHandleWindows : public IFileHandle
	{
	public:
		explicit FFileHandleWindows(HANDLE InFileHandle)
			: FileHandle(InFileHandle)
		{
		}

		virtual ~FFileHandleWindows() override
		{
			CloseHandle(FileHandle);
		}

		virtual int64 Tell() override
		{
			LARGE_INTEGER Zero;
			Zero.QuadPart = 0;
			LARGE_INTEGER Position;
			return SetFilePointerEx(FileHandle, Zero, &Position, FILE_CURRENT) ? Position.QuadPart : -1;
		}

		virtual bool Seek(int64 NewPosition) override
		{
			LARGE_INTEGER Distance;
			Distance.QuadPart = NewPosition;
			return SetFilePointerEx(FileHandle, Distance, nullptr, FILE_BEGIN) != FALSE;
		}

		virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd) override
		{
			LARGE_INTEGER Distance;
			Distance.QuadPart = NewPositionRelativeToEnd;
			return SetFilePointerEx(FileHandle, Distance, nullptr, FILE_END) != FALSE;
		}

		virtual bool Read(uint8* Destination, int64 BytesToRead) override
		{
			while (BytesToRead > 0)
			{
				const DWORD ThisSize = DWORD(FMath::Min<int64>(BytesToRead, 0x40000000));
				DWORD Result = 0;
				if (!ReadFile(FileHandle, Destination, ThisSize, &Result, nullptr) || Result != ThisSize)
				{
					return false;
				}
				Destination += ThisSize;
				BytesToRead -= ThisSize;
			}
			return true;
		}

		virtual bool Write(const uint8* Source, int64 BytesToWrite) override
		{
			while (BytesToWrite > 0)
			{
				const DWORD ThisSize = DWORD(FMath::Min<int64>(BytesToWrite, 0x40000000));
				DWORD Result = 0;
				if (!WriteFile(FileHandle, Source, ThisSize, &Result, nullptr) || Result != ThisSize)
				{
					return false;
				}
				Source += ThisSize;
				BytesToWrite -= ThisSize;
			}
			return true;
		}

		virtual bool Flush(const bool bFullFlush) override
		{
			return bFullFlush ? FlushFileBuffers(FileHandle) != FALSE : true;
		}

		virtual bool Truncate(int64 NewSize) override
		{
			return Seek(NewSize) && SetEndOfFile(FileHandle) != FALSE;
		}

		virtual int64 Size() override
		{
			LARGE_INTEGER FileSize;
			return GetFileSizeEx(FileHandle, &FileSize) ? FileSize.QuadPart : -1;
		}

	private:
		HANDLE FileHandle;
	};

	class FWindowsPlatformFile : public IPhysicalPlatformFile
	{
	public:
		using IPlatformFile::IterateDirectory;

		virtual bool FileExists(const TCHAR* Filename) override
		{
			const DWORD Result = GetFileAttributesW(TCHAR_TO_WCHAR(Filename));
			return Result != INVALID_FILE_ATTRIBUTES && !(Result & FILE_ATTRIBUTE_DIRECTORY);
		}

		virtual int64 FileSize(const TCHAR* Filename) override
		{
			WIN32_FILE_ATTRIBUTE_DATA Info;
			if (GetFileAttributesExW(TCHAR_TO_WCHAR(Filename), GetFileExInfoStandard, &Info) &&
				!(Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				return (int64(Info.nFileSizeHigh) << 32) | int64(Info.nFileSizeLow);
			}
			return -1;
		}

		virtual bool DeleteFile(const TCHAR* Filename) override
		{
			return DeleteFileW(TCHAR_TO_WCHAR(Filename)) != FALSE;
		}

		virtual bool IsReadOnly(const TCHAR* Filename) override
		{
			const DWORD Result = GetFileAttributesW(TCHAR_TO_WCHAR(Filename));
			return Result != INVALID_FILE_ATTRIBUTES && (Result & FILE_ATTRIBUTE_READONLY);
		}

		virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override
		{
			const FTCHARToWide ToWide(To);
			return MoveFileW(TCHAR_TO_WCHAR(From), ToWide.Get()) != FALSE;
		}

		virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override
		{
			const FTCHARToWide FilenameWide(Filename);
			const DWORD Attributes = GetFileAttributesW(FilenameWide.Get());
			if (Attributes == INVALID_FILE_ATTRIBUTES)
			{
				return false;
			}
			const DWORD NewAttributes = bNewReadOnlyValue ? (Attributes | FILE_ATTRIBUTE_READONLY)
														  : (Attributes & ~DWORD(FILE_ATTRIBUTE_READONLY));
			return SetFileAttributesW(FilenameWide.Get(), NewAttributes) != FALSE;
		}

		virtual FDateTime GetTimeStamp(const TCHAR* Filename) override
		{
			WIN32_FILE_ATTRIBUTE_DATA Info;
			if (GetFileAttributesExW(TCHAR_TO_WCHAR(Filename), GetFileExInfoStandard, &Info))
			{
				return FromFileTime(Info.ftLastWriteTime);
			}
			return FDateTime::MinValue();
		}

		virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite) override
		{
			const DWORD ShareMode = FILE_SHARE_READ | (bAllowWrite ? FILE_SHARE_WRITE : 0);
			HANDLE Handle = CreateFileW(TCHAR_TO_WCHAR(Filename), GENERIC_READ, ShareMode, nullptr, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL, nullptr);
			return Handle != INVALID_HANDLE_VALUE ? new FFileHandleWindows(Handle) : nullptr;
		}

		virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead) override
		{
			const DWORD Access = GENERIC_WRITE | (bAllowRead ? GENERIC_READ : 0);
			const DWORD ShareMode = bAllowRead ? FILE_SHARE_READ : 0;
			const DWORD Creation = bAppend ? OPEN_ALWAYS : CREATE_ALWAYS;
			HANDLE Handle = CreateFileW(
				TCHAR_TO_WCHAR(Filename), Access, ShareMode, nullptr, Creation, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (Handle == INVALID_HANDLE_VALUE)
			{
				return nullptr;
			}
			FFileHandleWindows* FileHandle = new FFileHandleWindows(Handle);
			if (bAppend)
			{
				FileHandle->SeekFromEnd(0);
			}
			return FileHandle;
		}

		virtual bool DirectoryExists(const TCHAR* Directory) override
		{
			const FString Trimmed = TrimDirectory(Directory);
			const DWORD Result = GetFileAttributesW(TCHAR_TO_WCHAR(*Trimmed));
			return Result != INVALID_FILE_ATTRIBUTES && (Result & FILE_ATTRIBUTE_DIRECTORY);
		}

		virtual bool CreateDirectory(const TCHAR* Directory) override
		{
			const FString Trimmed = TrimDirectory(Directory);
			return CreateDirectoryW(TCHAR_TO_WCHAR(*Trimmed), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
		}

		virtual bool DeleteDirectory(const TCHAR* Directory) override
		{
			const FString Trimmed = TrimDirectory(Directory);
			RemoveDirectoryW(TCHAR_TO_WCHAR(*Trimmed));
			return !DirectoryExists(*Trimmed);
		}

		virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override
		{
			const FString Trimmed = TrimDirectory(FilenameOrDirectory);
			WIN32_FILE_ATTRIBUTE_DATA Info;
			if (!GetFileAttributesExW(TCHAR_TO_WCHAR(*Trimmed), GetFileExInfoStandard, &Info))
			{
				return FFileStatData();
			}
			const bool bIsDirectory = (Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			const int64 Size = bIsDirectory ? -1 : ((int64(Info.nFileSizeHigh) << 32) | int64(Info.nFileSizeLow));
			return FFileStatData(FromFileTime(Info.ftCreationTime), FromFileTime(Info.ftLastAccessTime),
				FromFileTime(Info.ftLastWriteTime), Size, bIsDirectory,
				(Info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0);
		}

		virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override
		{
			const FString Trimmed = TrimDirectory(Directory);
			const FString Pattern = Trimmed + "/*";

			WIN32_FIND_DATAW Data;
			HANDLE Handle = FindFirstFileW(TCHAR_TO_WCHAR(*Pattern), &Data);
			if (Handle == INVALID_HANDLE_VALUE)
			{
				return false;
			}

			bool bResult = true;
			do
			{
				if (wcscmp(Data.cFileName, L".") == 0 || wcscmp(Data.cFileName, L"..") == 0)
				{
					continue;
				}
				const FString FullPath = Trimmed + "/" + FString(Data.cFileName);
				bResult = Visitor.Visit(*FullPath, (Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
			} while (bResult && FindNextFileW(Handle, &Data));

			FindClose(Handle);
			return bResult;
		}
	};
} // namespace

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FWindowsPlatformFile Singleton;
	return Singleton;
}
