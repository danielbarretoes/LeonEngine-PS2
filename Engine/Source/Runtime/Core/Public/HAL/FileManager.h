#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/DateTime.h"

class FArchive;

/** Flags for IFileManager::CreateFileWriter (UE: EFileWrite). */
enum EFileWrite
{
	FILEWRITE_None = 0x00,
	FILEWRITE_NoFail = 0x01,
	FILEWRITE_NoReplaceExisting = 0x02,
	FILEWRITE_EvenIfReadOnly = 0x04,
	FILEWRITE_Append = 0x08,
	FILEWRITE_AllowRead = 0x10,
	FILEWRITE_Silent = 0x20
};

/** Flags for IFileManager::CreateFileReader (UE: EFileRead). */
enum EFileRead
{
	FILEREAD_None = 0x00,
	FILEREAD_NoFail = 0x01,
	FILEREAD_Silent = 0x02,
	FILEREAD_AllowWrite = 0x04
};

/**
 * File operations above the platform file: archives for reading and writing, and tree helpers (UE: IFileManager).
 * Everything goes through FPlatformFileManager's topmost platform file.
 */
class CORE_API IFileManager
{
public:
	static IFileManager& Get();

	virtual ~IFileManager() = default;

	/** An archive reading Filename, or nullptr when it cannot be opened (logged unless FILEREAD_Silent). */
	virtual FArchive* CreateFileReader(const TCHAR* Filename, uint32 ReadFlags = 0) = 0;

	/** An archive writing Filename; missing directories are created (UE: CreateFileWriter). */
	virtual FArchive* CreateFileWriter(const TCHAR* Filename, uint32 WriteFlags = 0) = 0;

	virtual bool IsReadOnly(const TCHAR* Filename) = 0;
	virtual bool Delete(
		const TCHAR* Filename, bool bRequireExists = false, bool bEvenReadOnly = false, bool bQuiet = false) = 0;

	/** Copies a file; creates the destination directory (UE: Copy). */
	virtual bool Copy(const TCHAR* Dest, const TCHAR* Src, bool bReplace = true, bool bEvenIfReadOnly = false) = 0;

	/** Renames a file; with bReplace an existing Dest is deleted first (UE: Move). */
	virtual bool Move(const TCHAR* Dest, const TCHAR* Src, bool bReplace = true, bool bEvenIfReadOnly = false) = 0;

	virtual bool FileExists(const TCHAR* Filename) = 0;
	virtual bool DirectoryExists(const TCHAR* InDirectory) = 0;

	/** Creates a directory; with bTree also the missing parents (UE: MakeDirectory). */
	virtual bool MakeDirectory(const TCHAR* Path, bool bTree = false) = 0;

	/** Deletes a directory; with bTree also everything inside (UE: DeleteDirectory). */
	virtual bool DeleteDirectory(const TCHAR* Path, bool bRequireExists = false, bool bTree = false) = 0;

	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) = 0;

	/**
	 * Names (not paths) of the entries matching a wildcard path such as "Config/Default*.ini" (UE: FindFiles(Result,
	 * Filename, Files, Directories)).
	 */
	virtual void FindFiles(TArray<FString>& FileNames, const TCHAR* Filename, bool bFiles, bool bDirectories) = 0;

	/** Names of the files of Directory with FileExtension ("ini" or ".ini"; null for all) (UE: FindFiles). */
	void FindFiles(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension = nullptr);

	/** Full paths under StartDirectory whose name matches the Filename wildcard (UE: FindFilesRecursive). */
	virtual void FindFilesRecursive(TArray<FString>& FileNames, const TCHAR* StartDirectory, const TCHAR* Filename,
		bool bFiles, bool bDirectories, bool bClearFileNames = true) = 0;

	virtual bool IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) = 0;
	virtual bool IterateDirectoryRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) = 0;

	/** Modification time, FDateTime::MinValue when missing. */
	virtual FDateTime GetTimeStamp(const TCHAR* Filename) = 0;

	/** Size in bytes, -1 when missing. */
	virtual int64 FileSize(const TCHAR* Filename) = 0;
};
