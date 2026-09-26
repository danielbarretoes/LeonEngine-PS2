#include "IPlatformFilePak.h"

#include "Containers/Set.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

DEFINE_LOG_CATEGORY(LogPakFile);

namespace
{
	/** Reads an entry's bytes from its pak (UE: FPakFileHandle, without the compression and encryption paths). */
	class FPakFileHandle final : public IFileHandle
	{
	public:
		FPakFileHandle(const TSharedPtr<FPakFile>& InPakFile, const FPakEntry& InEntry)
			: PakFile(InPakFile)
			, Entry(InEntry)
		{
		}

		virtual int64 Tell() override
		{
			return Position;
		}

		virtual bool Seek(int64 NewPosition) override
		{
			if (NewPosition < 0 || NewPosition > Entry.Size)
			{
				return false;
			}
			Position = NewPosition;
			return true;
		}

		virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd) override
		{
			return Seek(Entry.Size + NewPositionRelativeToEnd);
		}

		virtual bool Read(uint8* Destination, int64 BytesToRead) override
		{
			if (BytesToRead < 0 || Position + BytesToRead > Entry.Size)
			{
				return false;
			}
			if (BytesToRead > 0 && !PakFile->Read(Entry.Offset + Position, Destination, BytesToRead))
			{
				return false;
			}
			Position += BytesToRead;
			return true;
		}

		virtual bool Write(const uint8* /*Source*/, int64 /*BytesToWrite*/) override
		{
			return false;
		}

		virtual bool Flush(const bool /*bFullFlush*/) override
		{
			return false;
		}

		virtual bool Truncate(int64 /*NewSize*/) override
		{
			return false;
		}

		virtual int64 Size() override
		{
			return Entry.Size;
		}

	private:
		TSharedPtr<FPakFile> PakFile;
		FPakEntry Entry;
		int64 Position = 0;
	};

	/** A directory path with a single trailing '/'. */
	FString WithTrailingSlash(FString Path)
	{
		while (Path.EndsWith("/", ESearchCase::CaseSensitive) && Path.Len() > 1)
		{
			Path.LeftChopInline(1);
		}
		return Path + "/";
	}

	/** A path as the caller spelled it, without trailing separators (the prefix of the visited paths). */
	FString TrimDirectory(const TCHAR* Directory)
	{
		FString Result(Directory);
		Result.ReplaceCharInline('\\', '/');
		while (Result.Len() > 1 && Result.EndsWith("/", ESearchCase::CaseSensitive))
		{
			Result.LeftChopInline(1);
		}
		return Result;
	}

	/** A relative mount point is taken from the executable's folder, as UE's "../../../". */
	FString ResolveMountPoint(const FString& InMountPoint)
	{
		FString Result = InMountPoint;
		Result.ReplaceCharInline('\\', '/');
		if (FPaths::IsRelative(Result))
		{
			Result = FPaths::ConvertRelativePathToFull(FString(FPlatformProcess::BaseDir()), Result);
		}
		else
		{
			FPaths::CollapseRelativeDirectories(Result);
		}
		return WithTrailingSlash(Result);
	}
} // namespace

// FPakInfo

void FPakInfo::Serialize(FArchive& Ar)
{
	Ar << Magic;
	Ar << Version;
	Ar << IndexOffset;
	Ar << IndexSize;
	Ar << IndexHash;
}

// FPakFile

FPakFile::FPakFile(IPlatformFile* LowerLevel, const TCHAR* Filename)
	: PakFilename(Filename)
{
	if (LowerLevel != nullptr)
	{
		PakHandle.Reset(LowerLevel->OpenRead(Filename));
	}
	if (!PakHandle)
	{
		UE_LOG(LogPakFile, Error, "Pak file '%s' cannot be opened", Filename);
		return;
	}
	PakSize = PakHandle->Size();
	bIsValid = Initialize();
}

FPakFile::FPakFile(const TCHAR* Filename, TArray<uint8>&& PakData)
	: PakFilename(Filename)
	, MemoryData(MoveTemp(PakData))
{
	PakSize = MemoryData.Num();
	bIsValid = Initialize();
}

FPakFile::~FPakFile() = default;

bool FPakFile::Read(int64 Offset, uint8* Destination, int64 Length)
{
	if (Offset < 0 || Length < 0 || Offset + Length > PakSize)
	{
		return false;
	}
	if (!PakHandle)
	{
		FMemory::Memcpy(Destination, MemoryData.GetData() + Offset, SIZE_T(Length));
		return true;
	}
	return PakHandle->Seek(Offset) && PakHandle->Read(Destination, Length);
}

bool FPakFile::ReadEntry(const FPakEntry& Entry, TArray<uint8>& OutData)
{
	OutData.SetNumUninitialized(int32(Entry.Size));
	return Entry.Size == 0 || Read(Entry.Offset, OutData.GetData(), Entry.Size);
}

bool FPakFile::Initialize()
{
	const int64 FooterSize = FPakInfo::GetSerializedSize();
	if (PakSize < FooterSize)
	{
		UE_LOG(LogPakFile, Error, "'%s' is not a pak file (too small)", *PakFilename);
		return false;
	}
	TArray<uint8> FooterBytes;
	FooterBytes.SetNumUninitialized(int32(FooterSize));
	if (!Read(PakSize - FooterSize, FooterBytes.GetData(), FooterSize))
	{
		UE_LOG(LogPakFile, Error, "'%s': the footer cannot be read", *PakFilename);
		return false;
	}
	FMemoryReader FooterReader(FooterBytes);
	Info.Serialize(FooterReader);
	if (Info.Magic != FPakInfo::PakFile_Magic)
	{
		UE_LOG(LogPakFile, Error, "'%s' is not a pak file (no LPAK magic)", *PakFilename);
		return false;
	}
	if (Info.Version < FPakInfo::PakFile_Version_Initial || Info.Version > FPakInfo::PakFile_Version_Latest)
	{
		UE_LOG(LogPakFile, Error, "'%s' has the unknown pak version %d", *PakFilename, Info.Version);
		return false;
	}
	if (Info.IndexOffset < 0 || Info.IndexSize < 0 || Info.IndexOffset + Info.IndexSize != PakSize - FooterSize ||
		Info.IndexSize > MAX_int32)
	{
		UE_LOG(LogPakFile, Error, "'%s': the index is out of the file", *PakFilename);
		return false;
	}

	// The index, checked against its hash before anything is taken from it (UE does the same at mount).
	TArray<uint8> IndexBytes;
	IndexBytes.SetNumUninitialized(int32(Info.IndexSize));
	if (!Read(Info.IndexOffset, IndexBytes.GetData(), Info.IndexSize))
	{
		UE_LOG(LogPakFile, Error, "'%s': the index cannot be read", *PakFilename);
		return false;
	}
	if (FSHA1::HashBuffer(IndexBytes.GetData(), uint64(IndexBytes.Num())) != Info.IndexHash)
	{
		UE_LOG(LogPakFile, Error, "'%s': the index is corrupt (its SHA-1 does not match)", *PakFilename);
		return false;
	}
	FMemoryReader IndexReader(IndexBytes);
	IndexReader << IndexMountPoint;
	int32 NumEntries = 0;
	IndexReader << NumEntries;
	if (IndexReader.IsError() || NumEntries < 0)
	{
		UE_LOG(LogPakFile, Error, "'%s': the index is malformed", *PakFilename);
		return false;
	}
	Entries.Reserve(NumEntries);
	for (int32 Index = 0; Index < NumEntries && !IndexReader.IsError(); ++Index)
	{
		FPakIndexEntry& Entry = Entries.AddDefaulted_GetRef();
		IndexReader << Entry.PathHash;
		IndexReader << Entry.Filename;
		IndexReader << Entry.Entry.Offset;
		IndexReader << Entry.Entry.Size;
		IndexReader << Entry.Entry.Hash;
		if (Entry.Entry.Offset < 0 || Entry.Entry.Size < 0 ||
			Entry.Entry.Offset + Entry.Entry.Size > Info.IndexOffset || Entry.PathHash != HashPath(Entry.Filename))
		{
			UE_LOG(LogPakFile, Error, "'%s': entry %d is malformed", *PakFilename, Index);
			return false;
		}
		if (Index > 0 && !IndexLess(Entries[Index - 1], Entry))
		{
			UE_LOG(LogPakFile, Error, "'%s': the index is not sorted by path hash", *PakFilename);
			return false;
		}
	}
	if (IndexReader.IsError() || IndexReader.Tell() != IndexReader.TotalSize())
	{
		UE_LOG(LogPakFile, Error, "'%s': the index is malformed", *PakFilename);
		return false;
	}
	MountPoint = ResolveMountPoint(IndexMountPoint);
	return true;
}

void FPakFile::SetMountPoint(const TCHAR* InMountPoint)
{
	MountPoint = ResolveMountPoint(InMountPoint);
}

uint32 FPakFile::HashPath(const FString& RelativeFilename)
{
	FString Lower = RelativeFilename.ToLower();
	Lower.ReplaceCharInline('\\', '/');
	return FCrc::MemCrc32(*Lower, Lower.Len());
}

bool FPakFile::IndexLess(const FPakIndexEntry& A, const FPakIndexEntry& B)
{
	if (A.PathHash != B.PathHash)
	{
		return A.PathHash < B.PathHash;
	}
	return A.Filename.ToLower().Compare(B.Filename.ToLower(), ESearchCase::CaseSensitive) < 0;
}

FString FPakFile::NormalizePath(const TCHAR* Filename)
{
	return FPaths::ConvertRelativePathToFull(FString(Filename));
}

const FPakIndexEntry* FPakFile::FindRelative(const FString& RelativeFilename) const
{
	const uint32 Hash = HashPath(RelativeFilename);
	// The first entry whose hash is not below Hash, then every entry of that hash (a collision compares the paths).
	int32 Low = 0;
	int32 High = Entries.Num();
	while (Low < High)
	{
		const int32 Middle = Low + (High - Low) / 2;
		if (Entries[Middle].PathHash < Hash)
		{
			Low = Middle + 1;
		}
		else
		{
			High = Middle;
		}
	}
	for (int32 Index = Low; Index < Entries.Num() && Entries[Index].PathHash == Hash; ++Index)
	{
		if (Entries[Index].Filename.Equals(RelativeFilename, ESearchCase::IgnoreCase))
		{
			return &Entries[Index];
		}
	}
	return nullptr;
}

const FPakIndexEntry* FPakFile::Find(const FString& FullFilename) const
{
	if (!FullFilename.StartsWith(MountPoint, ESearchCase::IgnoreCase))
	{
		return nullptr;
	}
	return FindRelative(FullFilename.RightChop(MountPoint.Len()));
}

bool FPakFile::DirectoryExists(const FString& FullDirectory) const
{
	const FString Directory = WithTrailingSlash(FullDirectory);
	// The mount point and every folder above it hold the pak's entries.
	if (MountPoint.StartsWith(Directory, ESearchCase::IgnoreCase))
	{
		return Entries.Num() > 0;
	}
	if (!Directory.StartsWith(MountPoint, ESearchCase::IgnoreCase))
	{
		return false;
	}
	const FString Relative = Directory.RightChop(MountPoint.Len());
	for (const FPakIndexEntry& Entry : Entries)
	{
		if (Entry.Filename.StartsWith(Relative, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

bool FPakFile::Check()
{
	int32 Errors = 0;
	TArray<uint8> Data;
	for (const FPakIndexEntry& Entry : Entries)
	{
		if (!ReadEntry(Entry.Entry, Data))
		{
			UE_LOG(LogPakFile, Error, "'%s': \"%s\" cannot be read", *PakFilename, *Entry.Filename);
			++Errors;
			continue;
		}
		const FSHAHash Hash = FSHA1::HashBuffer(Data.GetData(), uint64(Data.Num()));
		if (Hash != Entry.Entry.Hash)
		{
			UE_LOG(LogPakFile, Error, "'%s': \"%s\" is corrupt: SHA-1 %s, the index says %s", *PakFilename,
				*Entry.Filename, *Hash.ToString(), *Entry.Entry.Hash.ToString());
			++Errors;
		}
	}
	return Errors == 0;
}

// FPakPlatformFile

FPakPlatformFile::FPakPlatformFile() = default;

FPakPlatformFile::~FPakPlatformFile() = default;

void FPakPlatformFile::GetPakFolders(TArray<FString>& OutPakFolders)
{
	OutPakFolders.Add(FPaths::ProjectContentDir() + "Paks/");
	const FString EnginePaks = FPaths::EngineContentDir() + "Paks/";
	if (!FPaths::IsSamePath(EnginePaks, OutPakFolders[0]))
	{
		OutPakFolders.Add(EnginePaks);
	}
}

void FPakPlatformFile::FindAllPakFiles(IPlatformFile* LowerLevelFile, TArray<FString>& OutPakFiles)
{
	TArray<FString> Folders;
	GetPakFolders(Folders);
	for (const FString& Folder : Folders)
	{
		TArray<FString> Found;
		LowerLevelFile->FindFiles(Found, *Folder, ".lpak");
		Found.Sort([](const FString& A, const FString& B)
			{ return A.ToLower().Compare(B.ToLower(), ESearchCase::CaseSensitive) < 0; });
		OutPakFiles.Append(Found);
	}
}

bool FPakPlatformFile::ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const
{
#if UE_BUILD_SHIPPING
	// A Shipping build reads its content from paks only: the platform file is mandatory, -NoPak included.
	(void)Inner;
	(void)CmdLine;
	return true;
#else
	if (FParse::Param(CmdLine, "NoPak"))
	{
		return false;
	}
	if (FParse::Param(CmdLine, "Pak"))
	{
		return true;
	}
	TArray<FString> FoundPakFiles;
	FindAllPakFiles(Inner, FoundPakFiles);
	return FoundPakFiles.Num() > 0;
#endif
}

bool FPakPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)
{
	LowerLevel = Inner;
#if UE_BUILD_SHIPPING
	bAllowLooseFiles = false;
#else
	bAllowLooseFiles = !FParse::Param(CmdLine, "NoLooseFiles");
#endif
	TArray<FString> PakFilenames;
	FindAllPakFiles(LowerLevel, PakFilenames);
	int32 NumMounted = 0;
	for (int32 Index = 0; Index < PakFilenames.Num(); ++Index)
	{
		// A later pak in the list mounts with a higher order: it wins a path both have (UE's pak order).
		NumMounted += Mount(*PakFilenames[Index], uint32(Index)) ? 1 : 0;
	}
	UE_LOG(LogPakFile, Log, "Mounted %d pak file(s)%s", NumMounted, bAllowLooseFiles ? "" : "; loose files refused");
	if (!bAllowLooseFiles && NumMounted == 0)
	{
		UE_LOG(LogPakFile, Error, "No .lpak found in %s: a build that refuses loose files reads only from its paks",
			*(FPaths::ProjectContentDir() + "Paks/"));
		return false;
	}
	return true;
}

bool FPakPlatformFile::AddPak(TUniquePtr<FPakFile>&& PakFile, uint32 PakOrder, const TCHAR* InPath)
{
	if (!PakFile->IsValid())
	{
		return false;
	}
	if (InPath != nullptr)
	{
		PakFile->SetMountPoint(InPath);
	}
	UE_LOG(LogPakFile, Log, "Mounted '%s' (%d files) at %s", *PakFile->GetFilename(), PakFile->GetNumFiles(),
		*PakFile->GetMountPoint());
	int32 InsertIndex = 0;
	while (InsertIndex < PakFiles.Num() && PakFiles[InsertIndex].ReadOrder > PakOrder)
	{
		++InsertIndex;
	}
	FPakListEntry Entry;
	Entry.ReadOrder = PakOrder;
	Entry.PakFile = TSharedPtr<FPakFile>(MakeShareable(PakFile.Release()));
	PakFiles.Insert(MoveTemp(Entry), InsertIndex);
	return true;
}

bool FPakPlatformFile::Mount(const TCHAR* InPakFilename, uint32 PakOrder, const TCHAR* InPath)
{
	if (LowerLevel == nullptr)
	{
		LowerLevel = &IPlatformFile::GetPlatformPhysical();
	}
	return AddPak(MakeUnique<FPakFile>(LowerLevel, InPakFilename), PakOrder, InPath);
}

bool FPakPlatformFile::MountFromMemory(
	const TCHAR* InPakFilename, TArray<uint8>&& PakData, uint32 PakOrder, const TCHAR* InPath)
{
	return AddPak(MakeUnique<FPakFile>(InPakFilename, MoveTemp(PakData)), PakOrder, InPath);
}

bool FPakPlatformFile::Unmount(const TCHAR* InPakFilename)
{
	for (int32 Index = 0; Index < PakFiles.Num(); ++Index)
	{
		if (FPaths::IsSamePath(PakFiles[Index].PakFile->GetFilename(), InPakFilename))
		{
			PakFiles.RemoveAt(Index);
			return true;
		}
	}
	return false;
}

void FPakPlatformFile::GetMountedPakFilenames(TArray<FString>& OutPakFilenames) const
{
	for (const FPakListEntry& Entry : PakFiles)
	{
		OutPakFilenames.Add(Entry.PakFile->GetFilename());
	}
}

FPakFile* FPakPlatformFile::FindFileInPakFiles(const TCHAR* Filename, const FPakIndexEntry** OutEntry) const
{
	if (PakFiles.Num() == 0)
	{
		return nullptr;
	}
	const FString Normalized = FPakFile::NormalizePath(Filename);
	for (const FPakListEntry& Entry : PakFiles)
	{
		if (const FPakIndexEntry* Found = Entry.PakFile->Find(Normalized))
		{
			if (OutEntry != nullptr)
			{
				*OutEntry = Found;
			}
			return Entry.PakFile.Get();
		}
	}
	return nullptr;
}

bool FPakPlatformFile::IsNonPakFilenameAllowed(const FString& InFilename) const
{
	if (bAllowLooseFiles)
	{
		return true;
	}
	// A build that refuses loose files still reads what it wrote: its logs, the user config layer, screenshots.
	const FString Normalized = FPakFile::NormalizePath(*InFilename);
	return FPaths::IsUnderDirectory(Normalized, FPaths::ProjectSavedDir()) ||
		FPaths::IsUnderDirectory(Normalized, FPaths::EngineSavedDir());
}

bool FPakPlatformFile::DirectoryExistsInPakFiles(const FString& NormalizedDirectory) const
{
	for (const FPakListEntry& Entry : PakFiles)
	{
		if (Entry.PakFile->DirectoryExists(NormalizedDirectory))
		{
			return true;
		}
	}
	return false;
}

bool FPakPlatformFile::FileExists(const TCHAR* Filename)
{
	if (FindFileInPakFiles(Filename) != nullptr)
	{
		return true;
	}
	return IsNonPakFilenameAllowed(Filename) && LowerLevel->FileExists(Filename);
}

int64 FPakPlatformFile::FileSize(const TCHAR* Filename)
{
	const FPakIndexEntry* Entry = nullptr;
	if (FindFileInPakFiles(Filename, &Entry) != nullptr)
	{
		return Entry->Entry.Size;
	}
	return IsNonPakFilenameAllowed(Filename) ? LowerLevel->FileSize(Filename) : -1;
}

bool FPakPlatformFile::DeleteFile(const TCHAR* Filename)
{
	// A file in a pak cannot be deleted (UE).
	if (FindFileInPakFiles(Filename) != nullptr)
	{
		return false;
	}
	return LowerLevel->DeleteFile(Filename);
}

bool FPakPlatformFile::IsReadOnly(const TCHAR* Filename)
{
	if (FindFileInPakFiles(Filename) != nullptr)
	{
		return true;
	}
	return LowerLevel->IsReadOnly(Filename);
}

bool FPakPlatformFile::MoveFile(const TCHAR* To, const TCHAR* From)
{
	if (FindFileInPakFiles(From) != nullptr)
	{
		return false;
	}
	return LowerLevel->MoveFile(To, From);
}

bool FPakPlatformFile::SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue)
{
	if (FindFileInPakFiles(Filename) != nullptr)
	{
		return bNewReadOnlyValue;
	}
	return LowerLevel->SetReadOnly(Filename, bNewReadOnlyValue);
}

FDateTime FPakPlatformFile::GetPakTimeStamp(const FPakFile& PakFile) const
{
	// The .lpak on the lower level (a pak lives outside Saved, so the loose-file rule does not apply to it).
	const FDateTime PakTime = LowerLevel->GetTimeStamp(*PakFile.GetFilename());
	return PakTime == FDateTime::MinValue() ? FDateTime(2000, 1, 1) : PakTime;
}

FDateTime FPakPlatformFile::GetTimeStamp(const TCHAR* Filename)
{
	// A file in a pak has the pak's time stamp (UE), so it never looks changed (the shader hot reload).
	if (const FPakFile* PakFile = FindFileInPakFiles(Filename))
	{
		return GetPakTimeStamp(*PakFile);
	}
	return IsNonPakFilenameAllowed(Filename) ? LowerLevel->GetTimeStamp(Filename) : FDateTime::MinValue();
}

IFileHandle* FPakPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	const FPakIndexEntry* Entry = nullptr;
	if (FPakFile* PakFile = FindFileInPakFiles(Filename, &Entry))
	{
		for (const FPakListEntry& Listed : PakFiles)
		{
			if (Listed.PakFile.Get() == PakFile)
			{
				return new FPakFileHandle(Listed.PakFile, Entry->Entry);
			}
		}
	}
	return IsNonPakFilenameAllowed(Filename) ? LowerLevel->OpenRead(Filename, bAllowWrite) : nullptr;
}

IFileHandle* FPakPlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
	// Files in a pak are read-only (UE).
	if (FindFileInPakFiles(Filename) != nullptr)
	{
		return nullptr;
	}
	return LowerLevel->OpenWrite(Filename, bAppend, bAllowRead);
}

bool FPakPlatformFile::DirectoryExists(const TCHAR* Directory)
{
	if (DirectoryExistsInPakFiles(FPakFile::NormalizePath(Directory)))
	{
		return true;
	}
	return IsNonPakFilenameAllowed(Directory) && LowerLevel->DirectoryExists(Directory);
}

bool FPakPlatformFile::CreateDirectory(const TCHAR* Directory)
{
	return LowerLevel->CreateDirectory(Directory);
}

bool FPakPlatformFile::DeleteDirectory(const TCHAR* Directory)
{
	// A folder of a pak cannot be deleted (UE).
	if (DirectoryExistsInPakFiles(FPakFile::NormalizePath(Directory)))
	{
		return false;
	}
	return LowerLevel->DeleteDirectory(Directory);
}

FFileStatData FPakPlatformFile::GetStatData(const TCHAR* FilenameOrDirectory)
{
	const FPakIndexEntry* Entry = nullptr;
	if (FPakFile* PakFile = FindFileInPakFiles(FilenameOrDirectory, &Entry))
	{
		const FDateTime PakTime = GetPakTimeStamp(*PakFile);
		return FFileStatData(PakTime, PakTime, PakTime, Entry->Entry.Size, false, true);
	}
	if (DirectoryExistsInPakFiles(FPakFile::NormalizePath(FilenameOrDirectory)))
	{
		return FFileStatData(FDateTime::MinValue(), FDateTime::MinValue(), FDateTime::MinValue(), -1, true, true);
	}
	return IsNonPakFilenameAllowed(FilenameOrDirectory) ? LowerLevel->GetStatData(FilenameOrDirectory)
														: FFileStatData();
}

bool FPakPlatformFile::IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor)
{
	// The paks' entries under the folder first (files and the next level of folders, each name once), then the loose
	// ones the paks do not have (UE's FPakVisitor).
	const FString Prefix = TrimDirectory(Directory);
	const FString Normalized = WithTrailingSlash(FPakFile::NormalizePath(Directory));
	TSet<FString> Visited;
	bool bFound = false;
	for (const FPakListEntry& PakEntry : PakFiles)
	{
		const FPakFile& PakFile = *PakEntry.PakFile;
		const FString& MountPoint = PakFile.GetMountPoint();
		if (MountPoint.StartsWith(Normalized, ESearchCase::IgnoreCase))
		{
			// The folder is at or above the mount point: its child on the way to the mount point is a folder.
			if (MountPoint.Len() > Normalized.Len() && PakFile.GetNumFiles() > 0)
			{
				FString Child = MountPoint.RightChop(Normalized.Len());
				Child = Child.Left(Child.Find("/", ESearchCase::CaseSensitive));
				bFound = true;
				if (!Visited.Contains(Child))
				{
					Visited.Add(Child);
					if (!Visitor.Visit(*(Prefix + "/" + Child), true))
					{
						return false;
					}
				}
				continue;
			}
		}
		else if (!Normalized.StartsWith(MountPoint, ESearchCase::IgnoreCase))
		{
			continue;
		}
		const FString Relative = Normalized.RightChop(MountPoint.Len());
		for (const FPakIndexEntry& Entry : PakFile.GetEntries())
		{
			if (!Entry.Filename.StartsWith(Relative, ESearchCase::IgnoreCase))
			{
				continue;
			}
			bFound = true;
			const FString Rest = Entry.Filename.RightChop(Relative.Len());
			const int32 Slash = Rest.Find("/", ESearchCase::CaseSensitive);
			const FString Child = Slash == INDEX_NONE ? Rest : Rest.Left(Slash);
			if (Visited.Contains(Child))
			{
				continue;
			}
			Visited.Add(Child);
			if (!Visitor.Visit(*(Prefix + "/" + Child), Slash != INDEX_NONE))
			{
				return false;
			}
		}
	}
	if (!IsNonPakFilenameAllowed(Directory))
	{
		return bFound;
	}

	class FLooseVisitor final : public FDirectoryVisitor
	{
	public:
		FLooseVisitor(FDirectoryVisitor& InVisitor, const TSet<FString>& InVisited)
			: Inner(InVisitor)
			, Visited(InVisited)
		{
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (Visited.Contains(FPaths::GetCleanFilename(FString(FilenameOrDirectory))))
			{
				return true;
			}
			bStopped = !Inner.Visit(FilenameOrDirectory, bIsDirectory);
			return !bStopped;
		}

		bool bStopped = false;

	private:
		FDirectoryVisitor& Inner;
		const TSet<FString>& Visited;
	};
	FLooseVisitor LooseVisitor(Visitor, Visited);
	const bool bLower = LowerLevel->IterateDirectory(Directory, LooseVisitor);
	return !LooseVisitor.bStopped && (bLower || bFound);
}
