#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformProcess.h"
#include "IPlatformFilePak.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PakWriter.h"

// The pak tool (UE: UnrealPak):
//   LeonPak <out.lpak> -create=<response file> [-align=<bytes>]   one "<source path>" "<path in the pak>" per line
//   LeonPak <in.lpak> -list                                        the mount point and every entry
//   LeonPak <in.lpak> -test                                        checks every entry's SHA-1
//   LeonPak <in.lpak> -extract=<dir>                               writes every entry under <dir>
// Exit code 0 on success, 1 otherwise.

namespace
{
	int32 Usage()
	{
		UE_LOG(LogPakFile, Display, "Usage:");
		UE_LOG(LogPakFile, Display, "  LeonPak <out.lpak> -create=<response file> [-align=<bytes>]");
		UE_LOG(LogPakFile, Display, "  LeonPak <in.lpak> -list | -test | -extract=<dir>");
		UE_LOG(
			LogPakFile, Display, "A response file lists one file per line: \"<source path>\" \"<path in the pak>\".");
		return 1;
	}

	int32 CreatePak(const FString& PakFilename, const FString& ResponseFile, int64 Alignment)
	{
		TArray<FPakInputPair> Pairs;
		if (!FPakWriter::ReadResponseFile(*ResponseFile, Pairs))
		{
			return 1;
		}
		FPakWriter Writer(Alignment);
		for (const FPakInputPair& Pair : Pairs)
		{
			if (!Writer.AddFileFromDisk(Pair))
			{
				return 1;
			}
		}
		if (!Writer.WriteToFile(*PakFilename))
		{
			return 1;
		}
		UE_LOG(LogPakFile, Display, "Created %s: %d file(s), %lld bytes%s", *PakFilename, Writer.GetNumFiles(),
			(long long)IFileManager::Get().FileSize(*PakFilename),
			Alignment > 1 ? *FString::Printf(TEXT(", aligned to %lld"), (long long)Alignment) : TEXT(""));
		return 0;
	}

	/** The entries sorted by path, for listing and extracting. */
	TArray<const FPakIndexEntry*> SortedEntries(const FPakFile& PakFile)
	{
		TArray<const FPakIndexEntry*> Sorted;
		for (const FPakIndexEntry& Entry : PakFile.GetEntries())
		{
			Sorted.Add(&Entry);
		}
		Sorted.Sort([](const FPakIndexEntry& A, const FPakIndexEntry& B)
			{ return A.Filename.ToLower().Compare(B.Filename.ToLower(), ESearchCase::CaseSensitive) < 0; });
		return Sorted;
	}

	int32 ListPak(FPakFile& PakFile)
	{
		UE_LOG(LogPakFile, Display, "Mount point %s", *PakFile.GetIndexMountPoint());
		int64 TotalSize = 0;
		for (const FPakIndexEntry* Entry : SortedEntries(PakFile))
		{
			UE_LOG(LogPakFile, Display, "\"%s\" offset: %lld, size: %lld bytes, sha1: %s", *Entry->Filename,
				(long long)Entry->Entry.Offset, (long long)Entry->Entry.Size, *Entry->Entry.Hash.ToString());
			TotalSize += Entry->Entry.Size;
		}
		UE_LOG(LogPakFile, Display, "%d file(s) (%lld bytes), version %d, index %lld bytes at %lld",
			PakFile.GetNumFiles(), (long long)TotalSize, PakFile.GetInfo().Version,
			(long long)PakFile.GetInfo().IndexSize, (long long)PakFile.GetInfo().IndexOffset);
		return 0;
	}

	int32 TestPak(FPakFile& PakFile)
	{
		if (!PakFile.Check())
		{
			UE_LOG(LogPakFile, Error, "%s: the pak is corrupt", *PakFile.GetFilename());
			return 1;
		}
		UE_LOG(LogPakFile, Display, "%s: %d file(s) checked, every SHA-1 matches", *PakFile.GetFilename(),
			PakFile.GetNumFiles());
		return 0;
	}

	int32 ExtractPak(FPakFile& PakFile, const FString& InDestination)
	{
		const FString Destination = FPaths::ConvertRelativePathToFull(InDestination);
		int32 Errors = 0;
		TArray<uint8> Data;
		for (const FPakIndexEntry* Entry : SortedEntries(PakFile))
		{
			// An entry must land inside the destination: no absolute path, no ".." out of it (a crafted pak).
			FString Filename = FPaths::Combine(Destination, Entry->Filename);
			if (!FPaths::IsRelative(Entry->Filename) || !FPaths::CollapseRelativeDirectories(Filename) ||
				!FPaths::IsUnderDirectory(Filename, Destination))
			{
				UE_LOG(LogPakFile, Error, "\"%s\" is outside the destination: not extracted", *Entry->Filename);
				++Errors;
				continue;
			}
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			if (!PakFile.ReadEntry(Entry->Entry, Data) || !FFileHelper::SaveArrayToFile(Data, *Filename))
			{
				UE_LOG(LogPakFile, Error, "\"%s\" cannot be extracted to '%s'", *Entry->Filename, *Filename);
				++Errors;
			}
		}
		UE_LOG(LogPakFile, Display, "Extracted %d file(s) to %s", PakFile.GetNumFiles() - Errors, *Destination);
		return Errors == 0 ? 0 : 1;
	}

	int32 Run()
	{
		const TCHAR* CmdLine = FCommandLine::Get();
		const TCHAR* Stream = CmdLine;
		const FString PakFilename = FParse::Token(Stream, false);
		if (PakFilename.IsEmpty() || PakFilename.StartsWith("-"))
		{
			return Usage();
		}

		FString ResponseFile;
		if (FParse::Value(CmdLine, "create=", ResponseFile))
		{
			int64 Alignment = 0;
			FString AlignText;
			if (FParse::Value(CmdLine, "align=", AlignText))
			{
				Alignment = FCString::Atoi64(*AlignText);
				if (Alignment < 1)
				{
					UE_LOG(LogPakFile, Error, "-align= needs a positive byte count");
					return 1;
				}
			}
			return CreatePak(PakFilename, ResponseFile, Alignment);
		}

		FPakFile PakFile(&FPlatformFileManager::Get().GetPlatformFile(), *PakFilename);
		if (!PakFile.IsValid())
		{
			return 1;
		}
		FString ExtractDir;
		if (FParse::Param(CmdLine, "list"))
		{
			return ListPak(PakFile);
		}
		if (FParse::Param(CmdLine, "test"))
		{
			return TestPak(PakFile);
		}
		if (FParse::Value(CmdLine, "extract=", ExtractDir))
		{
			return ExtractPak(PakFile, ExtractDir);
		}
		return Usage();
	}
} // namespace

int main(int ArgC, char* ArgV[])
{
	FPlatformProcess::SetArgV0(ArgV[0]);
	FCommandLine::Set(*FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr));
	FModuleManager::Get().StartupStaticallyLinkedModules();
	const int32 ExitCode = Run();
	GLog->Flush();
	FModuleManager::Get().ShutdownModules();
	return ExitCode;
}
