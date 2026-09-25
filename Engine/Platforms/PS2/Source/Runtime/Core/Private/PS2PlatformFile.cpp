#include "GenericPlatform/GenericPlatformFile.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/DateTime.h"

#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Read-only file access for the EE through the ps2sdk newlib port, which routes POSIX calls to the IOP by device
// prefix: "host:" (PCSX2 HostFS, the ELF's folder), "cdrom0:" (disc), "mass:" (USB). Writes fail: saving to the
// memory card is a later milestone.

namespace
{
	FString TrimDirectory(const TCHAR* Directory)
	{
		FString Result(Directory);
		while (Result.Len() > 1 && Result[Result.Len() - 1] == '/' && Result[Result.Len() - 2] != ':')
		{
			Result.LeftChopInline(1);
		}
		return Result;
	}

	class FFileHandlePS2 : public IFileHandle
	{
	public:
		explicit FFileHandlePS2(int32 InFileHandle)
			: FileHandle(InFileHandle)
		{
		}

		virtual ~FFileHandlePS2() override
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
				const int32 Result =
					int32(read(FileHandle, Destination, size_t(FMath::Min<int64>(BytesToRead, 0x100000))));
				if (Result <= 0)
				{
					return false;
				}
				Destination += Result;
				BytesToRead -= Result;
			}
			return true;
		}

		virtual bool Write(const uint8* /*Source*/, int64 /*BytesToWrite*/) override
		{
			return false;
		}

		virtual bool Flush(const bool /*bFullFlush*/) override
		{
			return true;
		}

		virtual bool Truncate(int64 /*NewSize*/) override
		{
			return false;
		}

	private:
		int32 FileHandle;
	};

	class FPS2PlatformFile : public IPhysicalPlatformFile
	{
	public:
		using IPlatformFile::IterateDirectory;

		virtual bool FileExists(const TCHAR* Filename) override
		{
			// HostFS and the CD driver do not all implement stat: opening is the portable test.
			const int32 Handle = open(Filename, O_RDONLY);
			if (Handle < 0)
			{
				return false;
			}
			close(Handle);
			return true;
		}

		virtual int64 FileSize(const TCHAR* Filename) override
		{
			const int32 Handle = open(Filename, O_RDONLY);
			if (Handle < 0)
			{
				return -1;
			}
			const int64 Size = int64(lseek(Handle, 0, SEEK_END));
			close(Handle);
			return Size;
		}

		virtual bool DeleteFile(const TCHAR* /*Filename*/) override
		{
			return false;
		}

		virtual bool IsReadOnly(const TCHAR* Filename) override
		{
			return FileExists(Filename);
		}

		virtual bool MoveFile(const TCHAR* /*To*/, const TCHAR* /*From*/) override
		{
			return false;
		}

		virtual bool SetReadOnly(const TCHAR* /*Filename*/, bool /*bNewReadOnlyValue*/) override
		{
			return false;
		}

		virtual FDateTime GetTimeStamp(const TCHAR* Filename) override
		{
			// No calendar clock on the device side either; existing files report the fixed PS2 epoch.
			return FileExists(Filename) ? FDateTime(2000, 1, 1) : FDateTime::MinValue();
		}

		virtual IFileHandle* OpenRead(const TCHAR* Filename, bool /*bAllowWrite*/) override
		{
			const int32 Handle = open(Filename, O_RDONLY);
			return Handle >= 0 ? new FFileHandlePS2(Handle) : nullptr;
		}

		virtual IFileHandle* OpenWrite(const TCHAR* /*Filename*/, bool /*bAppend*/, bool /*bAllowRead*/) override
		{
			return nullptr;
		}

		virtual bool DirectoryExists(const TCHAR* Directory) override
		{
			const FString Trimmed = TrimDirectory(Directory);
			DIR* Handle = opendir(*Trimmed);
			if (Handle == nullptr)
			{
				return false;
			}
			closedir(Handle);
			return true;
		}

		virtual bool CreateDirectory(const TCHAR* Directory) override
		{
			return DirectoryExists(Directory);
		}

		virtual bool DeleteDirectory(const TCHAR* /*Directory*/) override
		{
			return false;
		}

		virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override
		{
			if (DirectoryExists(FilenameOrDirectory))
			{
				return FFileStatData(
					FDateTime(2000, 1, 1), FDateTime(2000, 1, 1), FDateTime(2000, 1, 1), -1, true, true);
			}
			const int64 Size = FileSize(FilenameOrDirectory);
			if (Size < 0)
			{
				return FFileStatData();
			}
			return FFileStatData(
				FDateTime(2000, 1, 1), FDateTime(2000, 1, 1), FDateTime(2000, 1, 1), Size, false, true);
		}

		virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override
		{
			const FString Trimmed = TrimDirectory(Directory);
			DIR* Handle = opendir(*Trimmed);
			if (Handle == nullptr)
			{
				return false;
			}

			const bool bNeedsSlash =
				Trimmed.Len() > 0 && Trimmed[Trimmed.Len() - 1] != '/' && Trimmed[Trimmed.Len() - 1] != ':';
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
				const FString FullPath = Trimmed + (bNeedsSlash ? "/" : "") + Entry->d_name;
				bResult = Visitor.Visit(*FullPath, DirectoryExists(*FullPath));
			}

			closedir(Handle);
			return bResult;
		}
	};
} // namespace

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FPS2PlatformFile Singleton;
	return Singleton;
}
