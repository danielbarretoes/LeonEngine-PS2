#pragma once

// .lpak files and the platform file that reads them (UE: Runtime/PakFile/Public/IPlatformFilePak.h).

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/SecureHash.h"
#include "Templates/UniquePtr.h"

PAKFILE_API DECLARE_LOG_CATEGORY_EXTERN(LogPakFile, Log, All);

/**
 * The footer at the very end of a `.lpak` file (UE: FPakInfo, trimmed: no encryption, no compression methods). Byte
 * layout, little-endian, 44 bytes: Magic (uint32), Version (int32), IndexOffset (int64), IndexSize (int64), IndexHash
 * (20 bytes, the SHA-1 of the index). Docs/ASSET_FORMATS.md has the whole file.
 */
struct PAKFILE_API FPakInfo
{
	enum
	{
		/** The bytes "LPAK" read as a little-endian uint32 (UE: PakFile_Magic, 0x5A6F12E1). */
		PakFile_Magic = 0x4B41504C,

		/** 0.17.0 (P16): the first layout. */
		PakFile_Version_Initial = 1,
		PakFile_Version_Latest = PakFile_Version_Initial,
	};

	uint32 Magic = PakFile_Magic;
	int32 Version = PakFile_Version_Latest;
	/** Where the index starts: right after the last entry's data. */
	int64 IndexOffset = -1;
	int64 IndexSize = 0;
	/** The SHA-1 of the IndexSize bytes at IndexOffset. */
	FSHAHash IndexHash;

	/** The footer's size on disk (UE: GetSerializedSize). */
	static constexpr int64 GetSerializedSize()
	{
		return 4 + 4 + 8 + 8 + 20;
	}

	void Serialize(FArchive& Ar);
};

/**
 * Where a file's bytes are in the pak (UE: FPakEntry, without compression blocks, encryption or the per-entry header UE
 * writes in front of the data: a Leon entry's bytes are stored raw at Offset).
 */
struct PAKFILE_API FPakEntry
{
	/** From the start of the pak file; a multiple of the pak's alignment (LeonPak -align=). */
	int64 Offset = -1;
	int64 Size = 0;
	/** The SHA-1 of the Size bytes (UE: Hash); LeonPak -test and FPakFile::Check verify it. */
	FSHAHash Hash;
};

/** One entry of a pak's index: its path relative to the mount point, and the path's hash (Leon). */
struct PAKFILE_API FPakIndexEntry
{
	/** FPakFile::HashPath of Filename: the index is sorted by it. */
	uint32 PathHash = 0;
	/** Relative to the pak's mount point, with '/' ("Engine/Content/Maps/Entry.lmap"). */
	FString Filename;
	FPakEntry Entry;
};

/**
 * An open `.lpak` file (UE: FPakFile). Opening reads the footer and the index and checks the index's SHA-1; the file
 * bytes are read on demand through the lower-level platform file (or from memory for a pak mounted from bytes).
 *
 * The index is sorted by the CRC-32 of each path, lowercased (HashPath), then by the lowercased path: a lookup is a
 * binary search on the hash, and the path is compared to resolve a collision. Lookups ignore case, as UE's do.
 */
class PAKFILE_API FPakFile
{
public:
	/** Opens Filename through LowerLevel (the platform file under the pak one). */
	FPakFile(IPlatformFile* LowerLevel, const TCHAR* Filename);

	/** A pak held in memory (Leon: the tests, and TestPAL on the PS2, whose platform file is read-only). */
	FPakFile(const TCHAR* Filename, TArray<uint8>&& PakData);

	~FPakFile();

	FPakFile(const FPakFile&) = delete;
	FPakFile& operator=(const FPakFile&) = delete;

	/** False when the file could not be read or is not a valid pak (logged) (UE: IsValid). */
	[[nodiscard]] bool IsValid() const
	{
		return bIsValid;
	}

	[[nodiscard]] const FString& GetFilename() const
	{
		return PakFilename;
	}

	[[nodiscard]] const FPakInfo& GetInfo() const
	{
		return Info;
	}

	/** The mount point the index stores ("../../../"; UE: the pak's MountPoint as written by UnrealPak). */
	[[nodiscard]] const FString& GetIndexMountPoint() const
	{
		return IndexMountPoint;
	}

	/**
	 * The folder the entries appear under, absolute, ending in '/' (UE: GetMountPoint). By default the index's mount
	 * point; a relative one is taken from the executable's folder (FPlatformProcess::BaseDir), so "../../../" is the
	 * folder above <Project>/Binaries/<Platform>/, as in UE.
	 */
	[[nodiscard]] const FString& GetMountPoint() const
	{
		return MountPoint;
	}

	/** Mounts the entries under another folder (UE: SetMountPoint); a relative folder is taken from BaseDir. */
	void SetMountPoint(const TCHAR* InMountPoint);

	/** The index, sorted by path hash. */
	[[nodiscard]] const TArray<FPakIndexEntry>& GetEntries() const
	{
		return Entries;
	}

	[[nodiscard]] int32 GetNumFiles() const
	{
		return Entries.Num();
	}

	/** The entry of a path relative to the mount point, or nullptr (case-insensitive). */
	[[nodiscard]] const FPakIndexEntry* FindRelative(const FString& RelativeFilename) const;

	/** The entry of a full path under the mount point (normalized: '/', absolute), or nullptr (UE: Find). */
	[[nodiscard]] const FPakIndexEntry* Find(const FString& FullFilename) const;

	/** Whether a full, normalized directory path holds entries of this pak (UE: DirectoryExistsInPak). */
	[[nodiscard]] bool DirectoryExists(const FString& FullDirectory) const;

	/** Reads Length bytes at Offset in the pak file; false on a short read. */
	bool Read(int64 Offset, uint8* Destination, int64 Length);

	/** Reads a whole entry. */
	bool ReadEntry(const FPakEntry& Entry, TArray<uint8>& OutData);

	/** Verifies every entry's SHA-1 against its bytes; logs each mismatch as an error (UE: Check). */
	bool Check();

	/** The hash the index is sorted by: CRC-32 (FCrc::MemCrc32) of the path, lowercased, with '/' (Leon). */
	static uint32 HashPath(const FString& RelativeFilename);

	/** Paths compare as the index sorts them: hash, then the lowercased path. */
	static bool IndexLess(const FPakIndexEntry& A, const FPakIndexEntry& B);

	/** A full path: '\' to '/', relative to the working directory, "." and ".." collapsed. */
	static FString NormalizePath(const TCHAR* Filename);

private:
	/** Reads the footer and the index; false (logged) when the pak is not valid. */
	bool Initialize();

	FString PakFilename;
	/** The pak on disk, or null for a pak in memory. */
	TUniquePtr<IFileHandle> PakHandle;
	TArray<uint8> MemoryData;
	int64 PakSize = 0;
	FPakInfo Info;
	FString IndexMountPoint;
	FString MountPoint;
	TArray<FPakIndexEntry> Entries;
	bool bIsValid = false;
};

/**
 * The platform file that serves the mounted paks (UE: FPakPlatformFile), a wrapper in the IPlatformFile chain above the
 * physical one: a read asks the paks first (the last mounted with the highest order wins) and then, when loose files
 * are allowed, the lower level. Writes, deletes and moves go to the lower level, except for a file in a pak.
 *
 * - Startup: FEngineLoop::PreInit puts it on top of the chain when ShouldBeUsed (a pak in one of GetPakFolders, `-pak`;
 *   always in Shipping) and Initialize mounts every `*.lpak` of those folders, sorted by name, before the config loads.
 * - Loose files: allowed outside Shipping. A Shipping build reads only from its paks (a missing pak is an error) and
 *   refuses a loose read, except under the Saved folders (logs, the user config layer, screenshots).
 */
class PAKFILE_API FPakPlatformFile : public IPlatformFile
{
public:
	FPakPlatformFile();
	virtual ~FPakPlatformFile() override;

	/** "PakFile" (UE: GetTypeName). */
	static const TCHAR* GetTypeName()
	{
		return "PakFile";
	}

	/** The folders searched for `*.lpak` at startup: <Project>/Content/Paks/ and Engine/Content/Paks/ (UE). */
	static void GetPakFolders(TArray<FString>& OutPakFolders);

	/** The `*.lpak` files of GetPakFolders, sorted by name (UE: FindAllPakFiles). */
	static void FindAllPakFiles(IPlatformFile* LowerLevelFile, TArray<FString>& OutPakFiles);

	/** Mounts a pak file; InPath replaces the pak's mount point. False when it cannot be read (UE: Mount). */
	bool Mount(const TCHAR* InPakFilename, uint32 PakOrder = 0, const TCHAR* InPath = nullptr);

	/** Mounts a pak held in memory (Leon: the tests and TestPAL). */
	bool MountFromMemory(
		const TCHAR* InPakFilename, TArray<uint8>&& PakData, uint32 PakOrder = 0, const TCHAR* InPath = nullptr);

	/** Unmounts a pak by file name (UE: Unmount). */
	bool Unmount(const TCHAR* InPakFilename);

	/** The mounted paks' file names, highest order first (UE: GetMountedPakFilenames). */
	void GetMountedPakFilenames(TArray<FString>& OutPakFilenames) const;

	/** The mounted pak serving a file, and its entry, or nullptr (UE: FindFileInPakFiles). */
	FPakFile* FindFileInPakFiles(const TCHAR* Filename, const FPakIndexEntry** OutEntry = nullptr) const;

	/** Whether a file outside the paks may be read (UE: IsNonPakFilenameAllowed). */
	[[nodiscard]] bool IsNonPakFilenameAllowed(const FString& InFilename) const;

	/** Allows or refuses the loose reads (Leon; Initialize sets it: refused in Shipping). */
	void SetAllowLooseFiles(bool bInAllowLooseFiles)
	{
		bAllowLooseFiles = bInAllowLooseFiles;
	}

	[[nodiscard]] bool AreLooseFilesAllowed() const
	{
		return bAllowLooseFiles;
	}

	// IPlatformFile

	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const override;
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override;
	virtual IPlatformFile* GetLowerLevel() override
	{
		return LowerLevel;
	}
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override
	{
		LowerLevel = NewLowerLevel;
	}
	virtual const TCHAR* GetName() const override
	{
		return GetTypeName();
	}
	virtual bool FileExists(const TCHAR* Filename) override;
	virtual int64 FileSize(const TCHAR* Filename) override;
	virtual bool DeleteFile(const TCHAR* Filename) override;
	virtual bool IsReadOnly(const TCHAR* Filename) override;
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override;
	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override;
	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override;
	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override;
	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override;
	virtual bool DirectoryExists(const TCHAR* Directory) override;
	virtual bool CreateDirectory(const TCHAR* Directory) override;
	virtual bool DeleteDirectory(const TCHAR* Directory) override;
	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override;
	using IPlatformFile::IterateDirectory;
	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override;

private:
	struct FPakListEntry
	{
		uint32 ReadOrder = 0;
		TUniquePtr<FPakFile> PakFile;
	};

	/** Adds an opened pak, keeping the list sorted by order (the highest first). */
	bool AddPak(TUniquePtr<FPakFile>&& PakFile, uint32 PakOrder, const TCHAR* InPath);

	/** Whether a (normalized) directory exists in a pak. */
	bool DirectoryExistsInPakFiles(const FString& NormalizedDirectory) const;

	IPlatformFile* LowerLevel = nullptr;
	TArray<FPakListEntry> PakFiles;
	bool bAllowLooseFiles = true;
};
