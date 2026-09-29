#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformMisc.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"

#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Read-only file access for the EE through the ps2sdk newlib port, which routes POSIX calls to the IOP by device
// prefix: "host:" (PCSX2 HostFS, the ELF's folder), "cdrom0:" (disc), "mass:" (USB). Writes fail: saving to the
// memory card is a later milestone. The disc's ISO 9660 file system knows its files by 8.3 upper-case names with a
// ";1" version, so a cdrom0: path is asked for as the disc names it (FPaths::ToIso9660Path; BuildCookRun -iso lays the
// disc out with the same rule, Docs/PLANS/ps2-shipping.md N23).

namespace
{
	/** Holds the IOP lock for a call (FPS2PlatformMisc::LockIop): the IO thread and the game thread take turns. */
	struct FIopLockScope
	{
		FIopLockScope()
		{
			FPlatformMisc::LockIop();
		}
		~FIopLockScope()
		{
			FPlatformMisc::UnlockIop();
		}
		FIopLockScope(const FIopLockScope&) = delete;
		FIopLockScope& operator=(const FIopLockScope&) = delete;
	};

	/** A file's descriptor opened under the lock. */
	int32 OpenLocked(const FString& Path)
	{
		FIopLockScope Lock;
		return int32(open(*Path, O_RDONLY));
	}

	void CloseLocked(int32 Handle)
	{
		FIopLockScope Lock;
		close(Handle);
	}

	int64 SeekLocked(int32 Handle, int64 Offset, int Whence)
	{
		FIopLockScope Lock;
		return int64(lseek(Handle, off_t(Offset), Whence));
	}

	/** The disc's name of a cdrom0: path (a file or a folder); any other device's path as it is. */
	FString ToDevicePath(const TCHAR* Path, bool bFile)
	{
		const FString Result(Path);
		return Result.StartsWith(TEXT("cdrom"), ESearchCase::IgnoreCase) ? FPaths::ToIso9660Path(Result, bFile)
																		 : Result;
	}

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
			CloseLocked(FileHandle);
		}

		virtual int64 Tell() override
		{
			if (Position < 0)
			{
				Position = SeekLocked(FileHandle, 0, SEEK_CUR);
			}
			return Position;
		}

		virtual bool Seek(int64 NewPosition) override
		{
			// Where the handle already is (the next read of a file read forward): no call to the IOP (ps2-shipping
			// N24b).
			if (NewPosition == Position)
			{
				return true;
			}
			Position = SeekLocked(FileHandle, NewPosition, SEEK_SET);
			return Position != -1;
		}

		virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd) override
		{
			Position = SeekLocked(FileHandle, NewPositionRelativeToEnd, SEEK_END);
			return Position != -1;
		}

		virtual bool Read(uint8* Destination, int64 BytesToRead) override
		{
			while (BytesToRead > 0)
			{
				int32 Result = 0;
				{
					FIopLockScope Lock;
					Result = int32(read(FileHandle, Destination, size_t(FMath::Min<int64>(BytesToRead, 0x100000))));
				}
				if (Result <= 0)
				{
					// Where it stopped is not known: the next Seek asks.
					Position = -1;
					return false;
				}
				Destination += Result;
				BytesToRead -= Result;
				Position = Position >= 0 ? Position + Result : -1;
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
		/** The file position as this handle moved it (open: 0), or -1 when unknown. */
		int64 Position = 0;
	};

	class FPS2PlatformFile : public IPhysicalPlatformFile
	{
	public:
		using IPlatformFile::IterateDirectory;

		virtual bool FileExists(const TCHAR* Filename) override
		{
			// HostFS and the CD driver do not all implement stat: opening is the portable test.
			const int32 Handle = OpenLocked(ToDevicePath(Filename, true));
			if (Handle < 0)
			{
				return false;
			}
			CloseLocked(Handle);
			return true;
		}

		virtual int64 FileSize(const TCHAR* Filename) override
		{
			const int32 Handle = OpenLocked(ToDevicePath(Filename, true));
			if (Handle < 0)
			{
				return -1;
			}
			const int64 Size = SeekLocked(Handle, 0, SEEK_END);
			CloseLocked(Handle);
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
			const int32 Handle = OpenLocked(ToDevicePath(Filename, true));
			return Handle >= 0 ? new FFileHandlePS2(Handle) : nullptr;
		}

		virtual IFileHandle* OpenWrite(const TCHAR* /*Filename*/, bool /*bAppend*/, bool /*bAllowRead*/) override
		{
			return nullptr;
		}

		virtual bool DirectoryExists(const TCHAR* Directory) override
		{
			const FString Trimmed = TrimDirectory(*ToDevicePath(Directory, false));
			FIopLockScope Lock;
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
			const FString Trimmed = TrimDirectory(*ToDevicePath(Directory, false));
			DIR* Handle = nullptr;
			{
				FIopLockScope Lock;
				Handle = opendir(*Trimmed);
			}
			if (Handle == nullptr)
			{
				return false;
			}

			const bool bNeedsSlash =
				Trimmed.Len() > 0 && Trimmed[Trimmed.Len() - 1] != '/' && Trimmed[Trimmed.Len() - 1] != ':';
			bool bResult = true;
			while (bResult)
			{
				const dirent* Entry = nullptr;
				{
					FIopLockScope Lock;
					Entry = readdir(Handle);
				}
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

			{
				FIopLockScope Lock;
				closedir(Handle);
			}
			return bResult;
		}
	};
} // namespace

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FPS2PlatformFile Singleton;
	return Singleton;
}
