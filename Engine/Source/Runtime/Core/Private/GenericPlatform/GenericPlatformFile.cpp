#include "GenericPlatform/GenericPlatformFile.h"

#include "HAL/PlatformFilemanager.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UniquePtr.h"

namespace
{
	/** Directory without trailing separators ("C:/" and "/" keep theirs). */
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

	bool HasExtension(const TCHAR* Filename, const TCHAR* FileExtension)
	{
		if (FileExtension == nullptr || *FileExtension == 0)
		{
			return true;
		}
		const FString Name(Filename);
		const FString Extension = *FileExtension == '.' ? FString(FileExtension) : FString(".") + FileExtension;
		return Name.EndsWith(Extension);
	}
} // namespace

int64 IFileHandle::Size()
{
	const int64 Current = Tell();
	SeekFromEnd();
	const int64 Result = Tell();
	Seek(Current);
	return Result;
}

bool IPhysicalPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* /*CmdLine*/)
{
	// Physical platform files are the bottom of the chain.
	check(Inner == nullptr);
	return true;
}

void IPhysicalPlatformFile::SetLowerLevel(IPlatformFile* /*NewLowerLevel*/)
{
	checkf(false, "Physical platform file has no lower level");
}

const TCHAR* IPlatformFile::GetPhysicalTypeName()
{
	return "PhysicalFile";
}

bool IPlatformFile::IterateDirectory(const TCHAR* Directory, FDirectoryVisitorFunc Visitor)
{
	class FFunctionVisitor : public FDirectoryVisitor
	{
	public:
		explicit FFunctionVisitor(FDirectoryVisitorFunc InVisitorFunc)
			: VisitorFunc(InVisitorFunc)
		{
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			return VisitorFunc(FilenameOrDirectory, bIsDirectory);
		}

	private:
		FDirectoryVisitorFunc VisitorFunc;
	};

	FFunctionVisitor FunctionVisitor(Visitor);
	return IterateDirectory(Directory, FunctionVisitor);
}

bool IPlatformFile::IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitor& Visitor)
{
	class FRecurse : public FDirectoryVisitor
	{
	public:
		FRecurse(IPlatformFile& InPlatformFile, FDirectoryVisitor& InVisitor)
			: PlatformFile(InPlatformFile)
			, Visitor(InVisitor)
		{
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			bool bResult = Visitor.Visit(FilenameOrDirectory, bIsDirectory);
			if (bResult && bIsDirectory)
			{
				bResult = PlatformFile.IterateDirectory(FilenameOrDirectory, *this);
			}
			return bResult;
		}

	private:
		IPlatformFile& PlatformFile;
		FDirectoryVisitor& Visitor;
	};

	FRecurse Recurse(*this, Visitor);
	return IterateDirectory(Directory, Recurse);
}

bool IPlatformFile::IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitorFunc Visitor)
{
	class FFunctionVisitor : public FDirectoryVisitor
	{
	public:
		explicit FFunctionVisitor(FDirectoryVisitorFunc InVisitorFunc)
			: VisitorFunc(InVisitorFunc)
		{
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			return VisitorFunc(FilenameOrDirectory, bIsDirectory);
		}

	private:
		FDirectoryVisitorFunc VisitorFunc;
	};

	FFunctionVisitor FunctionVisitor(Visitor);
	return IterateDirectoryRecursively(Directory, FunctionVisitor);
}

bool IPlatformFile::DeleteDirectoryRecursively(const TCHAR* Directory)
{
	TArray<FString> Files;
	TArray<FString> Directories;
	IterateDirectoryRecursively(Directory,
		[&Files, &Directories](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			(bIsDirectory ? Directories : Files).Add(FilenameOrDirectory);
			return true;
		});

	for (const FString& File : Files)
	{
		SetReadOnly(*File, false);
		DeleteFile(*File);
	}

	// Deepest first: a child directory was visited after its parent.
	for (int32 Index = Directories.Num() - 1; Index >= 0; --Index)
	{
		DeleteDirectory(*Directories[Index]);
	}

	DeleteDirectory(Directory);
	return !DirectoryExists(Directory);
}

bool IPlatformFile::CreateDirectoryTree(const TCHAR* Directory)
{
	const FString LocalDirectory = TrimDirectory(Directory);
	if (LocalDirectory.IsEmpty() || DirectoryExists(*LocalDirectory))
	{
		return true;
	}

	// Create each missing level, from the root down. "C:" and device prefixes ("host:") are not directories.
	for (int32 Index = 1; Index <= LocalDirectory.Len(); ++Index)
	{
		if (Index == LocalDirectory.Len() || LocalDirectory[Index] == '/')
		{
			const FString Prefix = LocalDirectory.Mid(0, Index);
			if (Prefix.IsEmpty() || Prefix[Prefix.Len() - 1] == ':')
			{
				continue;
			}
			if (!DirectoryExists(*Prefix) && !CreateDirectory(*Prefix))
			{
				return false;
			}
		}
	}
	return DirectoryExists(*LocalDirectory);
}

bool IPlatformFile::CopyFile(const TCHAR* To, const TCHAR* From)
{
	TUniquePtr<IFileHandle> FromFile(OpenRead(From));
	if (!FromFile)
	{
		return false;
	}
	TUniquePtr<IFileHandle> ToFile(OpenWrite(To));
	if (!ToFile)
	{
		return false;
	}

	int64 Size = FromFile->Size();
	constexpr int64 BufferSize = 64 * 1024;
	uint8* Buffer = static_cast<uint8*>(FMemory::Malloc(SIZE_T(BufferSize)));
	bool bOk = true;
	while (Size > 0 && bOk)
	{
		const int64 ThisSize = Size < BufferSize ? Size : BufferSize;
		bOk = FromFile->Read(Buffer, ThisSize) && ToFile->Write(Buffer, ThisSize);
		Size -= ThisSize;
	}
	FMemory::Free(Buffer);
	return bOk && ToFile->Flush();
}

void IPlatformFile::FindFiles(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension)
{
	IterateDirectory(Directory,
		[&FoundFiles, FileExtension](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (!bIsDirectory && HasExtension(FilenameOrDirectory, FileExtension))
			{
				FoundFiles.Add(FilenameOrDirectory);
			}
			return true;
		});
}

void IPlatformFile::FindFilesRecursively(
	TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension)
{
	IterateDirectoryRecursively(Directory,
		[&FoundFiles, FileExtension](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (!bIsDirectory && HasExtension(FilenameOrDirectory, FileExtension))
			{
				FoundFiles.Add(FilenameOrDirectory);
			}
			return true;
		});
}
