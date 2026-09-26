#include "GenericPlatform/GenericPlatformFile.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/DateTime.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
	FDateTime FromUnixTime(time_t Seconds)
	{
		return FDateTime::FromUnixTimestamp(int64(Seconds));
	}

	FString TrimDirectory(const TCHAR* Directory)
	{
		FString Result(Directory);
		while (Result.Len() > 1 && Result[Result.Len() - 1] == '/')
		{
			Result.LeftChopInline(1);
		}
		return Result;
	}

	class FFileHandleLinux : public IFileHandle
	{
	public:
		explicit FFileHandleLinux(int32 InFileHandle)
			: FileHandle(InFileHandle)
		{
		}

		virtual ~FFileHandleLinux() override
		{
			close(FileHandle);
		}

		virtual int64 Tell() override
		{
			return int64(lseek(FileHandle, 0, SEEK_CUR));
		}

		virtual bool Seek(int64 NewPosition) override
		{
			return lseek(FileHandle, off_t(NewPosition), SEEK_SET) != -1;
		}

		virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd) override
		{
			return lseek(FileHandle, off_t(NewPositionRelativeToEnd), SEEK_END) != -1;
		}

		virtual bool Read(uint8* Destination, int64 BytesToRead) override
		{
			while (BytesToRead > 0)
			{
				const ssize_t Result =
					read(FileHandle, Destination, size_t(FMath::Min<int64>(BytesToRead, 0x40000000)));
				if (Result <= 0)
				{
					return false;
				}
				Destination += Result;
				BytesToRead -= Result;
			}
			return true;
		}

		virtual bool Write(const uint8* Source, int64 BytesToWrite) override
		{
			while (BytesToWrite > 0)
			{
				const ssize_t Result = write(FileHandle, Source, size_t(FMath::Min<int64>(BytesToWrite, 0x40000000)));
				if (Result <= 0)
				{
					return false;
				}
				Source += Result;
				BytesToWrite -= Result;
			}
			return true;
		}

		virtual bool Flush(const bool bFullFlush) override
		{
			return bFullFlush ? fsync(FileHandle) == 0 : true;
		}

		virtual bool Truncate(int64 NewSize) override
		{
			return ftruncate(FileHandle, off_t(NewSize)) == 0;
		}

		virtual int64 Size() override
		{
			struct stat FileInfo;
			return fstat(FileHandle, &FileInfo) == 0 ? int64(FileInfo.st_size) : -1;
		}

	private:
		int32 FileHandle;
	};

	class FLinuxPlatformFile : public IPhysicalPlatformFile
	{
	public:
		using IPlatformFile::IterateDirectory;

		virtual bool FileExists(const TCHAR* Filename) override
		{
			struct stat FileInfo;
			return stat(Filename, &FileInfo) == 0 && S_ISREG(FileInfo.st_mode);
		}

		virtual int64 FileSize(const TCHAR* Filename) override
		{
			struct stat FileInfo;
			return (stat(Filename, &FileInfo) == 0 && S_ISREG(FileInfo.st_mode)) ? int64(FileInfo.st_size) : -1;
		}

		virtual bool DeleteFile(const TCHAR* Filename) override
		{
			return unlink(Filename) == 0;
		}

		virtual bool IsReadOnly(const TCHAR* Filename) override
		{
			return access(Filename, F_OK) == 0 && access(Filename, W_OK) != 0;
		}

		virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override
		{
			struct stat FileInfo;
			if (stat(To, &FileInfo) == 0)
			{
				// Same contract as Windows' MoveFile: never replace.
				return false;
			}
			return rename(From, To) == 0;
		}

		virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override
		{
			struct stat FileInfo;
			if (stat(Filename, &FileInfo) != 0)
			{
				return false;
			}
			const mode_t Mode = bNewReadOnlyValue ? (FileInfo.st_mode & ~mode_t(S_IWUSR | S_IWGRP | S_IWOTH))
												  : (FileInfo.st_mode | S_IWUSR);
			return chmod(Filename, Mode) == 0;
		}

		virtual FDateTime GetTimeStamp(const TCHAR* Filename) override
		{
			struct stat FileInfo;
			return stat(Filename, &FileInfo) == 0 ? FromUnixTime(FileInfo.st_mtime) : FDateTime::MinValue();
		}

		virtual IFileHandle* OpenRead(const TCHAR* Filename, bool /*bAllowWrite*/) override
		{
			const int32 Handle = open(Filename, O_RDONLY | O_CLOEXEC);
			return Handle != -1 ? new FFileHandleLinux(Handle) : nullptr;
		}

		virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead) override
		{
			int Flags = O_CREAT | O_CLOEXEC | (bAllowRead ? O_RDWR : O_WRONLY);
			Flags |= bAppend ? O_APPEND : O_TRUNC;
			const int32 Handle = open(Filename, Flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			return Handle != -1 ? new FFileHandleLinux(Handle) : nullptr;
		}

		virtual bool DirectoryExists(const TCHAR* Directory) override
		{
			struct stat FileInfo;
			const FString Trimmed = TrimDirectory(Directory);
			return stat(*Trimmed, &FileInfo) == 0 && S_ISDIR(FileInfo.st_mode);
		}

		virtual bool CreateDirectory(const TCHAR* Directory) override
		{
			const FString Trimmed = TrimDirectory(Directory);
			return mkdir(*Trimmed, 0755) == 0 || errno == EEXIST;
		}

		virtual bool DeleteDirectory(const TCHAR* Directory) override
		{
			const FString Trimmed = TrimDirectory(Directory);
			return rmdir(*Trimmed) == 0;
		}

		virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override
		{
			struct stat FileInfo;
			const FString Trimmed = TrimDirectory(FilenameOrDirectory);
			if (stat(*Trimmed, &FileInfo) != 0)
			{
				return FFileStatData();
			}
			const bool bIsDirectory = S_ISDIR(FileInfo.st_mode);
			return FFileStatData(FromUnixTime(FileInfo.st_ctime), FromUnixTime(FileInfo.st_atime),
				FromUnixTime(FileInfo.st_mtime), bIsDirectory ? -1 : int64(FileInfo.st_size), bIsDirectory,
				access(*Trimmed, W_OK) != 0);
		}

		virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override
		{
			const FString Trimmed = TrimDirectory(Directory);
			DIR* Handle = opendir(*Trimmed);
			if (Handle == nullptr)
			{
				return false;
			}

			bool bResult = true;
			while (bResult)
			{
				const dirent* Entry = readdir(Handle);
				if (Entry == nullptr)
				{
					break;
				}
				if (strcmp(Entry->d_name, ".") == 0 || strcmp(Entry->d_name, "..") == 0)
				{
					continue;
				}
				const FString FullPath = Trimmed + "/" + Entry->d_name;
				struct stat FileInfo;
				const bool bIsDirectory = stat(*FullPath, &FileInfo) == 0 && S_ISDIR(FileInfo.st_mode);
				bResult = Visitor.Visit(*FullPath, bIsDirectory);
			}

			closedir(Handle);
			return bResult;
		}
	};
} // namespace

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FLinuxPlatformFile Singleton;
	return Singleton;
}
