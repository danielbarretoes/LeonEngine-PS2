#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "Serialization/Archive.h"
#include "Templates/UniquePtr.h"

/** IFileManager over the topmost platform file (UE: FFileManagerGeneric). */
class CORE_API FFileManagerGeneric : public IFileManager
{
public:
	using IFileManager::FindFiles;

	virtual FArchive* CreateFileReader(const TCHAR* Filename, uint32 ReadFlags = 0) override;
	virtual FArchive* CreateFileWriter(const TCHAR* Filename, uint32 WriteFlags = 0) override;
	virtual bool IsReadOnly(const TCHAR* Filename) override;
	virtual bool Delete(
		const TCHAR* Filename, bool bRequireExists = false, bool bEvenReadOnly = false, bool bQuiet = false) override;
	virtual bool Copy(const TCHAR* Dest, const TCHAR* Src, bool bReplace = true, bool bEvenIfReadOnly = false) override;
	virtual bool Move(const TCHAR* Dest, const TCHAR* Src, bool bReplace = true, bool bEvenIfReadOnly = false) override;
	virtual bool FileExists(const TCHAR* Filename) override;
	virtual bool DirectoryExists(const TCHAR* InDirectory) override;
	virtual bool MakeDirectory(const TCHAR* Path, bool bTree = false) override;
	virtual bool DeleteDirectory(const TCHAR* Path, bool bRequireExists = false, bool bTree = false) override;
	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override;
	virtual void FindFiles(TArray<FString>& FileNames, const TCHAR* Filename, bool bFiles, bool bDirectories) override;
	virtual void FindFilesRecursive(TArray<FString>& FileNames, const TCHAR* StartDirectory, const TCHAR* Filename,
		bool bFiles, bool bDirectories, bool bClearFileNames = true) override;
	virtual bool IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) override;
	virtual bool IterateDirectoryRecursively(
		const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) override;
	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override;
	virtual int64 FileSize(const TCHAR* Filename) override;

private:
	IPlatformFile& GetLowLevel() const;
};

/** Buffered archive reading an IFileHandle (UE: FArchiveFileReaderGeneric). */
class CORE_API FArchiveFileReaderGeneric : public FArchive
{
public:
	FArchiveFileReaderGeneric(IFileHandle* InHandle, const TCHAR* InFilename, int64 InSize);
	virtual ~FArchiveFileReaderGeneric() override;

	virtual void Seek(int64 InPos) override;
	virtual int64 Tell() override
	{
		return Pos;
	}
	virtual int64 TotalSize() override
	{
		return Size;
	}
	virtual bool Close() override;
	virtual void Serialize(void* V, int64 Length) override;
	virtual FString GetArchiveName() const override
	{
		return Filename;
	}

private:
	bool InternalPrecache(int64 PrecacheOffset, int64 PrecacheSize);

	static constexpr int64 BufferSize = 4096;

	FString Filename;
	int64 Size;
	int64 Pos;
	int64 BufferBase;
	int64 BufferCount;
	TUniquePtr<IFileHandle> Handle;
	uint8 Buffer[BufferSize];
};

/** Buffered archive writing an IFileHandle (UE: FArchiveFileWriterGeneric). */
class CORE_API FArchiveFileWriterGeneric : public FArchive
{
public:
	FArchiveFileWriterGeneric(IFileHandle* InHandle, const TCHAR* InFilename, int64 InPos);
	virtual ~FArchiveFileWriterGeneric() override;

	virtual void Seek(int64 InPos) override;
	virtual int64 Tell() override
	{
		return Pos;
	}
	virtual int64 TotalSize() override;
	virtual bool Close() override;
	virtual void Serialize(void* V, int64 Length) override;
	virtual void Flush() override;
	virtual FString GetArchiveName() const override
	{
		return Filename;
	}

private:
	bool FlushBuffer();

	static constexpr int64 BufferSize = 4096;

	FString Filename;
	int64 Pos;
	int64 BufferCount;
	TUniquePtr<IFileHandle> Handle;
	uint8 Buffer[BufferSize];
};
