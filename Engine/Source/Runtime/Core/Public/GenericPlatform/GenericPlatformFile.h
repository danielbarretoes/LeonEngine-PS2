#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Misc/DateTime.h"
#include "Templates/Function.h"

/** An open file (UE: IFileHandle). Delete it to close the file. */
class CORE_API IFileHandle
{
public:
	virtual ~IFileHandle() = default;

	/** Current position in bytes. */
	virtual int64 Tell() = 0;

	/** Moves to an absolute position. */
	virtual bool Seek(int64 NewPosition) = 0;

	/** Moves relative to the end (0 or negative). */
	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd = 0) = 0;

	/** Reads exactly BytesToRead bytes; false on a short read. */
	virtual bool Read(uint8* Destination, int64 BytesToRead) = 0;

	/** Writes exactly BytesToWrite bytes. */
	virtual bool Write(const uint8* Source, int64 BytesToWrite) = 0;

	virtual bool Flush(const bool bFullFlush = false) = 0;

	virtual bool Truncate(int64 NewSize) = 0;

	/** Size of the file in bytes; the generic version seeks to the end and back. */
	virtual int64 Size();
};

/** File or directory metadata (UE: FFileStatData). bIsValid is false when the path does not exist. */
struct FFileStatData
{
	FFileStatData()
		: CreationTime(FDateTime::MinValue())
		, AccessTime(FDateTime::MinValue())
		, ModificationTime(FDateTime::MinValue())
		, FileSize(-1)
		, bIsDirectory(false)
		, bIsReadOnly(false)
		, bIsValid(false)
	{
	}

	FFileStatData(FDateTime InCreationTime, FDateTime InAccessTime, FDateTime InModificationTime,
		const int64 InFileSize, const bool bInIsDirectory, const bool bInIsReadOnly)
		: CreationTime(InCreationTime)
		, AccessTime(InAccessTime)
		, ModificationTime(InModificationTime)
		, FileSize(InFileSize)
		, bIsDirectory(bInIsDirectory)
		, bIsReadOnly(bInIsReadOnly)
		, bIsValid(true)
	{
	}

	FDateTime CreationTime;
	FDateTime AccessTime;
	FDateTime ModificationTime;

	/** -1 for directories. */
	int64 FileSize;

	bool bIsDirectory;
	bool bIsReadOnly;
	bool bIsValid;
};

/**
 * File system access (UE: IPlatformFile). Platform files form a chain: the physical one at the bottom (Windows,
 * POSIX, PS2) and wrappers (the pak file in P16) on top; FPlatformFileManager hands out the topmost. Paths use '/'
 * and may be relative to the working directory; directories may end with '/'.
 */
class CORE_API IPlatformFile
{
public:
	/** The platform's physical file system (UE: GetPlatformPhysical). */
	static IPlatformFile& GetPlatformPhysical();

	/** Name of the physical platform file ("PhysicalFile"). */
	static const TCHAR* GetPhysicalTypeName();

	virtual ~IPlatformFile() = default;

	/** Whether this wrapper wants to sit on top of Inner for this command line. */
	virtual bool ShouldBeUsed(IPlatformFile* /*Inner*/, const TCHAR* /*CmdLine*/) const
	{
		return false;
	}

	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) = 0;
	virtual IPlatformFile* GetLowerLevel() = 0;
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) = 0;
	virtual const TCHAR* GetName() const = 0;

	virtual bool FileExists(const TCHAR* Filename) = 0;

	/** Size in bytes, -1 when the file does not exist. */
	virtual int64 FileSize(const TCHAR* Filename) = 0;

	virtual bool DeleteFile(const TCHAR* Filename) = 0;
	virtual bool IsReadOnly(const TCHAR* Filename) = 0;

	/** Renames From to To; fails when To exists (UE: MoveFile). */
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) = 0;

	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) = 0;

	/** Last modification time, FDateTime::MinValue when the file does not exist. */
	virtual FDateTime GetTimeStamp(const TCHAR* Filename) = 0;

	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) = 0;
	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) = 0;

	virtual bool DirectoryExists(const TCHAR* Directory) = 0;

	/** Creates one directory level; true when it exists afterwards. */
	virtual bool CreateDirectory(const TCHAR* Directory) = 0;

	/** Deletes an empty directory. */
	virtual bool DeleteDirectory(const TCHAR* Directory) = 0;

	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) = 0;

	/** Receives each entry of a directory: the full path ("Directory/Name") and whether it is a directory. */
	class CORE_API FDirectoryVisitor
	{
	public:
		virtual ~FDirectoryVisitor() = default;

		/** Return false to stop the iteration. */
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) = 0;
	};

	typedef TFunctionRef<bool(const TCHAR*, bool)> FDirectoryVisitorFunc;

	/** Visits the entries of one directory (not "." / ".."); false when the directory cannot be read or a visit stops.
	 */
	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) = 0;

	// Generic helpers on top of the virtual interface (UE: GenericPlatformFile.cpp).

	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitorFunc Visitor);
	virtual bool IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitor& Visitor);
	virtual bool IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitorFunc Visitor);

	/** Deletes a directory with everything inside it. */
	virtual bool DeleteDirectoryRecursively(const TCHAR* Directory);

	/** Creates a directory and any missing parents. */
	virtual bool CreateDirectoryTree(const TCHAR* Directory);

	/** Copies a file, replacing To. */
	virtual bool CopyFile(const TCHAR* To, const TCHAR* From);

	/** Files of Directory with the extension ("ini" or ".ini"; empty or null for every file), as full paths. */
	virtual void FindFiles(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension);
	virtual void FindFilesRecursively(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension);
};

/** Base of the physical platform files: nothing below them (UE: IPhysicalPlatformFile). */
class CORE_API IPhysicalPlatformFile : public IPlatformFile
{
public:
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override;

	virtual IPlatformFile* GetLowerLevel() override
	{
		return nullptr;
	}

	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override;

	virtual const TCHAR* GetName() const override
	{
		return IPlatformFile::GetPhysicalTypeName();
	}
};
