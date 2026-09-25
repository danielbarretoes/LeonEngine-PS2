#include "HAL/FileManagerGeneric.h"

#include "HAL/PlatformFilemanager.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogMacros.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"

DEFINE_LOG_CATEGORY_STATIC(LogFileManager, Log, All);

namespace
{
	/** Directory part of a path ("" when there is none). */
	FString GetDirectoryOf(const FString& Path)
	{
		int32 Slash = INDEX_NONE;
		FString Normalized = Path;
		Normalized.ReplaceCharInline('\\', '/');
		return Normalized.FindLastChar('/', Slash) ? Normalized.Mid(0, Slash) : FString();
	}

	/** '*' and '?' wildcard match, ignoring case like UE's FString::MatchesWildcard. */
	bool MatchesWildcard(const TCHAR* Name, const TCHAR* Pattern)
	{
		if (*Pattern == 0)
		{
			return *Name == 0;
		}
		if (*Pattern == '*')
		{
			for (const TCHAR* Rest = Name;; ++Rest)
			{
				if (MatchesWildcard(Rest, Pattern + 1))
				{
					return true;
				}
				if (*Rest == 0)
				{
					return false;
				}
			}
		}
		if (*Name == 0)
		{
			return false;
		}
		if (*Pattern == '?' || FChar::ToLower(*Pattern) == FChar::ToLower(*Name))
		{
			return MatchesWildcard(Name + 1, Pattern + 1);
		}
		return false;
	}

	/** Name part of a full path. */
	const TCHAR* GetCleanName(const TCHAR* Path)
	{
		const TCHAR* Name = Path;
		for (const TCHAR* Char = Path; *Char; ++Char)
		{
			if (*Char == '/' || *Char == '\\')
			{
				Name = Char + 1;
			}
		}
		return Name;
	}
} // namespace

IFileManager& IFileManager::Get()
{
	static FFileManagerGeneric Singleton;
	return Singleton;
}

void IFileManager::FindFiles(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension)
{
	FString Wildcard(Directory);
	if (!Wildcard.IsEmpty() && Wildcard[Wildcard.Len() - 1] != '/')
	{
		Wildcard += "/";
	}
	if (FileExtension == nullptr || *FileExtension == 0)
	{
		Wildcard += "*";
	}
	else
	{
		Wildcard += *FileExtension == '.' ? FString("*") + FileExtension : FString("*.") + FileExtension;
	}
	FindFiles(FoundFiles, *Wildcard, true, false);
}

IPlatformFile& FFileManagerGeneric::GetLowLevel() const
{
	return FPlatformFileManager::Get().GetPlatformFile();
}

FArchive* FFileManagerGeneric::CreateFileReader(const TCHAR* Filename, uint32 ReadFlags)
{
	IFileHandle* Handle = GetLowLevel().OpenRead(Filename, (ReadFlags & FILEREAD_AllowWrite) != 0);
	if (Handle == nullptr)
	{
		if (ReadFlags & FILEREAD_NoFail)
		{
			UE_LOG(LogFileManager, Fatal, "Failed to read file: %s", Filename);
		}
		return nullptr;
	}
	return new FArchiveFileReaderGeneric(Handle, Filename, Handle->Size());
}

FArchive* FFileManagerGeneric::CreateFileWriter(const TCHAR* Filename, uint32 WriteFlags)
{
	MakeDirectory(*GetDirectoryOf(Filename), true);

	if (WriteFlags & FILEWRITE_EvenIfReadOnly)
	{
		GetLowLevel().SetReadOnly(Filename, false);
	}

	IFileHandle* Handle = nullptr;
	if ((WriteFlags & FILEWRITE_NoReplaceExisting) && GetLowLevel().FileExists(Filename))
	{
		// Leave Handle null.
	}
	else
	{
		Handle = GetLowLevel().OpenWrite(
			Filename, (WriteFlags & FILEWRITE_Append) != 0, (WriteFlags & FILEWRITE_AllowRead) != 0);
	}

	if (Handle == nullptr)
	{
		if (WriteFlags & FILEWRITE_NoFail)
		{
			UE_LOG(LogFileManager, Fatal, "Failed to create file: %s", Filename);
		}
		else if (!(WriteFlags & FILEWRITE_Silent))
		{
			UE_LOG(LogFileManager, Warning, "Failed to create file: %s", Filename);
		}
		return nullptr;
	}
	return new FArchiveFileWriterGeneric(Handle, Filename, Handle->Tell());
}

bool FFileManagerGeneric::IsReadOnly(const TCHAR* Filename)
{
	return GetLowLevel().IsReadOnly(Filename);
}

bool FFileManagerGeneric::Delete(const TCHAR* Filename, bool bRequireExists, bool bEvenReadOnly, bool bQuiet)
{
	if (!GetLowLevel().FileExists(Filename))
	{
		return !bRequireExists;
	}
	if (bEvenReadOnly)
	{
		GetLowLevel().SetReadOnly(Filename, false);
	}
	if (!GetLowLevel().DeleteFile(Filename))
	{
		if (!bQuiet)
		{
			UE_LOG(LogFileManager, Warning, "Error deleting file: %s", Filename);
		}
		return false;
	}
	return true;
}

bool FFileManagerGeneric::Copy(const TCHAR* Dest, const TCHAR* Src, bool bReplace, bool bEvenIfReadOnly)
{
	if (!bReplace && GetLowLevel().FileExists(Dest))
	{
		return false;
	}
	if (bEvenIfReadOnly)
	{
		GetLowLevel().SetReadOnly(Dest, false);
	}
	MakeDirectory(*GetDirectoryOf(Dest), true);
	return GetLowLevel().CopyFile(Dest, Src);
}

bool FFileManagerGeneric::Move(const TCHAR* Dest, const TCHAR* Src, bool bReplace, bool bEvenIfReadOnly)
{
	MakeDirectory(*GetDirectoryOf(Dest), true);

	if (GetLowLevel().FileExists(Dest))
	{
		if (!bReplace)
		{
			return false;
		}
		if (!Delete(Dest, false, bEvenIfReadOnly, true))
		{
			return false;
		}
	}

	if (!GetLowLevel().MoveFile(Dest, Src))
	{
		UE_LOG(LogFileManager, Warning, "Error moving file '%s' to '%s'", Src, Dest);
		return false;
	}
	return true;
}

bool FFileManagerGeneric::FileExists(const TCHAR* Filename)
{
	return GetLowLevel().FileExists(Filename);
}

bool FFileManagerGeneric::DirectoryExists(const TCHAR* InDirectory)
{
	return GetLowLevel().DirectoryExists(InDirectory);
}

bool FFileManagerGeneric::MakeDirectory(const TCHAR* Path, bool bTree)
{
	if (*Path == 0)
	{
		return true;
	}
	return bTree ? GetLowLevel().CreateDirectoryTree(Path) : GetLowLevel().CreateDirectory(Path);
}

bool FFileManagerGeneric::DeleteDirectory(const TCHAR* Path, bool bRequireExists, bool bTree)
{
	if (!GetLowLevel().DirectoryExists(Path))
	{
		return !bRequireExists;
	}
	return bTree ? GetLowLevel().DeleteDirectoryRecursively(Path) : GetLowLevel().DeleteDirectory(Path);
}

FFileStatData FFileManagerGeneric::GetStatData(const TCHAR* FilenameOrDirectory)
{
	return GetLowLevel().GetStatData(FilenameOrDirectory);
}

void FFileManagerGeneric::FindFiles(TArray<FString>& FileNames, const TCHAR* Filename, bool bFiles, bool bDirectories)
{
	FString Directory = GetDirectoryOf(Filename);
	const FString Pattern = GetCleanName(Filename);
	if (Directory.IsEmpty())
	{
		Directory = ".";
	}

	GetLowLevel().IterateDirectory(*Directory,
		[&FileNames, &Pattern, bFiles, bDirectories](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if ((bIsDirectory ? bDirectories : bFiles))
			{
				const TCHAR* CleanName = GetCleanName(FilenameOrDirectory);
				if (MatchesWildcard(CleanName, *Pattern))
				{
					FileNames.Add(CleanName);
				}
			}
			return true;
		});
}

void FFileManagerGeneric::FindFilesRecursive(TArray<FString>& FileNames, const TCHAR* StartDirectory,
	const TCHAR* Filename, bool bFiles, bool bDirectories, bool bClearFileNames)
{
	if (bClearFileNames)
	{
		FileNames.Empty();
	}

	const FString Pattern(Filename);
	GetLowLevel().IterateDirectoryRecursively(StartDirectory,
		[&FileNames, &Pattern, bFiles, bDirectories](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if ((bIsDirectory ? bDirectories : bFiles) && MatchesWildcard(GetCleanName(FilenameOrDirectory), *Pattern))
			{
				FileNames.Add(FilenameOrDirectory);
			}
			return true;
		});
}

bool FFileManagerGeneric::IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor)
{
	return GetLowLevel().IterateDirectory(Directory, Visitor);
}

bool FFileManagerGeneric::IterateDirectoryRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor)
{
	return GetLowLevel().IterateDirectoryRecursively(Directory, Visitor);
}

FDateTime FFileManagerGeneric::GetTimeStamp(const TCHAR* Filename)
{
	return GetLowLevel().GetTimeStamp(Filename);
}

int64 FFileManagerGeneric::FileSize(const TCHAR* Filename)
{
	return GetLowLevel().FileSize(Filename);
}

// FArchiveFileReaderGeneric ------------------------------------------------------------------------------------------

FArchiveFileReaderGeneric::FArchiveFileReaderGeneric(IFileHandle* InHandle, const TCHAR* InFilename, int64 InSize)
	: Filename(InFilename)
	, Size(InSize)
	, Pos(0)
	, BufferBase(0)
	, BufferCount(0)
	, Handle(InHandle)
{
	SetIsLoading(true);
	SetIsPersistent(true);
}

FArchiveFileReaderGeneric::~FArchiveFileReaderGeneric()
{
	Close();
}

bool FArchiveFileReaderGeneric::InternalPrecache(int64 PrecacheOffset, int64 PrecacheSize)
{
	// Refill the buffer from the current position; the handle is always positioned at Pos here.
	check(PrecacheOffset == Pos);
	BufferBase = Pos;
	BufferCount = FMath::Max(FMath::Min(FMath::Min(PrecacheSize, BufferSize), Size - Pos), int64(0));
	if (BufferCount > 0 && !Handle->Read(Buffer, BufferCount))
	{
		BufferCount = 0;
		SetError();
		return false;
	}
	return true;
}

void FArchiveFileReaderGeneric::Seek(int64 InPos)
{
	check(InPos >= 0);
	if (!Handle->Seek(InPos))
	{
		SetError();
	}
	Pos = InPos;
	BufferBase = Pos;
	BufferCount = 0;
}

bool FArchiveFileReaderGeneric::Close()
{
	Handle.Reset();
	return !IsError();
}

void FArchiveFileReaderGeneric::Serialize(void* V, int64 Length)
{
	uint8* Dest = static_cast<uint8*>(V);
	while (Length > 0)
	{
		int64 Copy = FMath::Min(Length, BufferBase + BufferCount - Pos);
		if (Copy <= 0)
		{
			if (Length >= BufferSize)
			{
				// Large read: skip the buffer.
				if (Pos + Length > Size || !Handle->Read(Dest, Length))
				{
					SetError();
					FMemory::Memzero(Dest, SIZE_T(Length));
					return;
				}
				Pos += Length;
				BufferBase = Pos;
				BufferCount = 0;
				return;
			}
			if (!InternalPrecache(Pos, MAX_int32))
			{
				FMemory::Memzero(Dest, SIZE_T(Length));
				return;
			}
			Copy = FMath::Min(Length, BufferBase + BufferCount - Pos);
			if (Copy <= 0)
			{
				// Read past the end of the file.
				SetError();
				FMemory::Memzero(Dest, SIZE_T(Length));
				return;
			}
		}
		FMemory::Memcpy(Dest, Buffer + Pos - BufferBase, SIZE_T(Copy));
		Pos += Copy;
		Length -= Copy;
		Dest += Copy;
	}
}

// FArchiveFileWriterGeneric ------------------------------------------------------------------------------------------

FArchiveFileWriterGeneric::FArchiveFileWriterGeneric(IFileHandle* InHandle, const TCHAR* InFilename, int64 InPos)
	: Filename(InFilename)
	, Pos(InPos)
	, BufferCount(0)
	, Handle(InHandle)
{
	SetIsSaving(true);
	SetIsPersistent(true);
}

FArchiveFileWriterGeneric::~FArchiveFileWriterGeneric()
{
	Close();
}

void FArchiveFileWriterGeneric::Seek(int64 InPos)
{
	FlushBuffer();
	if (!Handle->Seek(InPos))
	{
		SetError();
	}
	Pos = InPos;
}

int64 FArchiveFileWriterGeneric::TotalSize()
{
	// Make sure that all data is written before looking at file size.
	FlushBuffer();
	return Handle->Size();
}

bool FArchiveFileWriterGeneric::Close()
{
	if (Handle)
	{
		if (!FlushBuffer() || !Handle->Flush())
		{
			SetError();
		}
		Handle.Reset();
	}
	return !IsError();
}

void FArchiveFileWriterGeneric::Serialize(void* V, int64 Length)
{
	Pos += Length;
	const uint8* Source = static_cast<const uint8*>(V);
	if (Length >= BufferSize)
	{
		// Large write: skip the buffer.
		FlushBuffer();
		if (!Handle->Write(Source, Length))
		{
			SetError();
		}
		return;
	}

	while (Length > 0)
	{
		const int64 Copy = FMath::Min(Length, BufferSize - BufferCount);
		FMemory::Memcpy(Buffer + BufferCount, Source, SIZE_T(Copy));
		BufferCount += Copy;
		Length -= Copy;
		Source += Copy;
		if (BufferCount == BufferSize)
		{
			FlushBuffer();
		}
	}
}

void FArchiveFileWriterGeneric::Flush()
{
	FlushBuffer();
	if (Handle)
	{
		Handle->Flush();
	}
}

bool FArchiveFileWriterGeneric::FlushBuffer()
{
	bool bSuccess = true;
	if (BufferCount > 0 && Handle)
	{
		bSuccess = Handle->Write(Buffer, BufferCount);
		if (!bSuccess)
		{
			SetError();
		}
		BufferCount = 0;
	}
	return bSuccess;
}
