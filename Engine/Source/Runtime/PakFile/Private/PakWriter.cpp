#include "PakWriter.h"

#include "HAL/FileManager.h"
#include "IPlatformFilePak.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"

FPakWriter::FPakWriter(int64 InAlignment)
	: Alignment(InAlignment > 1 ? InAlignment : 1)
{
}

void FPakWriter::AddFile(const FString& Dest, TArray<uint8>&& Data)
{
	FFile& File = Files.AddDefaulted_GetRef();
	File.Dest = Dest;
	File.Dest.ReplaceCharInline('\\', '/');
	File.Data = MoveTemp(Data);
}

bool FPakWriter::AddFileFromDisk(const FPakInputPair& Pair)
{
	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *Pair.Source))
	{
		UE_LOG(LogPakFile, Error, "'%s' cannot be read", *Pair.Source);
		return false;
	}
	AddFile(Pair.Dest, MoveTemp(Data));
	return true;
}

FString FPakWriter::ComputeMountPoint(const TArray<FString>& Dests)
{
	if (Dests.Num() == 0)
	{
		return FString();
	}
	// The folder of the first path, shortened until every path starts with it (UnrealPak's GetCommonRootPath).
	FString Common = FPaths::GetPath(Dests[0]);
	for (const FString& Dest : Dests)
	{
		while (!Common.IsEmpty() && !(Dest.StartsWith(Common + "/", ESearchCase::IgnoreCase)))
		{
			Common = FPaths::GetPath(Common);
		}
	}
	return Common.IsEmpty() ? FString() : Common + "/";
}

bool FPakWriter::Finalize(TArray<uint8>& OutPak) const
{
	OutPak.Reset();
	if (Files.Num() == 0)
	{
		UE_LOG(LogPakFile, Error, "A pak needs at least one file");
		return false;
	}
	TArray<FString> Dests;
	for (const FFile& File : Files)
	{
		Dests.Add(File.Dest);
	}
	const FString MountPoint = ComputeMountPoint(Dests);

	// The data in the open order, then the rest in path order, so the same files give the same bytes whatever order
	// they were added in.
	TArray<int32> Order;
	TArray<int64> Ranks;
	for (int32 Index = 0; Index < Files.Num(); ++Index)
	{
		Order.Add(Index);
		const int64* Rank = OpenOrder.Find(Files[Index].Dest.RightChop(MountPoint.Len()).ToLower());
		Ranks.Add(Rank != nullptr ? *Rank : MAX_int64);
	}
	Order.Sort(
		[this, &Ranks](int32 A, int32 B)
		{
			if (Ranks[A] != Ranks[B])
			{
				return Ranks[A] < Ranks[B];
			}
			return Files[A].Dest.ToLower().Compare(Files[B].Dest.ToLower(), ESearchCase::CaseSensitive) < 0;
		});

	TArray<FPakIndexEntry> Index;
	FMemoryWriter Writer(OutPak);
	TSet<FString> Seen;
	for (int32 Position = 0; Position < Order.Num(); ++Position)
	{
		const FFile& File = Files[Order[Position]];
		const FString Lower = File.Dest.ToLower();
		if (Seen.Contains(Lower))
		{
			UE_LOG(LogPakFile, Error, "\"%s\" is in the pak twice", *File.Dest);
			return false;
		}
		Seen.Add(Lower);
		// Zeros up to the alignment (the CD sector size), then the bytes as they are.
		const int64 Padding = (Alignment - Writer.Tell() % Alignment) % Alignment;
		for (int64 Pad = 0; Pad < Padding; ++Pad)
		{
			uint8 Zero = 0;
			Writer << Zero;
		}
		FPakIndexEntry& Entry = Index.AddDefaulted_GetRef();
		Entry.Filename = File.Dest.RightChop(MountPoint.Len());
		Entry.PathHash = FPakFile::HashPath(Entry.Filename);
		Entry.Entry.Offset = Writer.Tell();
		Entry.Entry.Size = File.Data.Num();
		Entry.Entry.Hash = FSHA1::HashBuffer(File.Data.GetData(), uint64(File.Data.Num()));
		Writer.Serialize(const_cast<uint8*>(File.Data.GetData()), File.Data.Num());
	}
	Index.Sort(&FPakFile::IndexLess);

	// The index: the mount point, the entry count, then each entry.
	TArray<uint8> IndexBytes;
	FMemoryWriter IndexWriter(IndexBytes);
	FString IndexMountPoint = MountPoint;
	IndexWriter << IndexMountPoint;
	int32 NumEntries = Index.Num();
	IndexWriter << NumEntries;
	for (FPakIndexEntry& Entry : Index)
	{
		IndexWriter << Entry.PathHash;
		IndexWriter << Entry.Filename;
		IndexWriter << Entry.Entry.Offset;
		IndexWriter << Entry.Entry.Size;
		IndexWriter << Entry.Entry.Hash;
	}

	FPakInfo Info;
	Info.IndexOffset = Writer.Tell();
	Info.IndexSize = IndexBytes.Num();
	Info.IndexHash = FSHA1::HashBuffer(IndexBytes.GetData(), uint64(IndexBytes.Num()));
	Writer.Serialize(IndexBytes.GetData(), IndexBytes.Num());
	Info.Serialize(Writer);
	return !Writer.IsError();
}

bool FPakWriter::WriteToFile(const TCHAR* Filename) const
{
	TArray<uint8> Pak;
	if (!Finalize(Pak))
	{
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FString(Filename)), true);
	if (!FFileHelper::SaveArrayToFile(Pak, Filename))
	{
		UE_LOG(LogPakFile, Error, "'%s' cannot be written", Filename);
		return false;
	}
	return true;
}

bool FPakWriter::ReadOrderFile(const TCHAR* OrderFile, TMap<FString, int64>& OutOpenOrder)
{
	OutOpenOrder.Reset();
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, OrderFile))
	{
		UE_LOG(LogPakFile, Error, "The order file '%s' cannot be read", OrderFile);
		return false;
	}
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, true);
	for (const FString& Line : Lines)
	{
		// The first quoted path, then the rank after it.
		const int32 Open = Line.Find("\"", ESearchCase::CaseSensitive);
		const int32 Close = Open == INDEX_NONE
			? INDEX_NONE
			: Line.Find("\"", ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1);
		if (Close == INDEX_NONE)
		{
			continue;
		}
		const FString Rank = Line.Mid(Close + 1).TrimStartAndEnd();
		if (Rank.IsEmpty() || !FChar::IsDigit(Rank[0]))
		{
			continue;
		}
		FString Path = Line.Mid(Open + 1, Close - Open - 1).ToLower();
		Path.ReplaceCharInline('\\', '/');
		if (!Path.IsEmpty() && !OutOpenOrder.Contains(Path))
		{
			OutOpenOrder.Add(Path, FCString::Atoi64(*Rank));
		}
	}
	return true;
}

bool FPakWriter::ReadResponseFile(const TCHAR* ResponseFile, TArray<FPakInputPair>& OutPairs)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, ResponseFile))
	{
		UE_LOG(LogPakFile, Error, "The response file '%s' cannot be read", ResponseFile);
		return false;
	}
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, true);
	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		const FString Line = Lines[LineIndex].TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(";") || Line.StartsWith("#"))
		{
			continue;
		}
		// Two tokens, each quoted or not: the source, then the path in the pak (UnrealPak's response files).
		TArray<FString> Tokens;
		int32 Position = 0;
		while (Position < Line.Len() && Tokens.Num() < 3)
		{
			while (Position < Line.Len() && (Line[Position] == ' ' || Line[Position] == '\t'))
			{
				++Position;
			}
			if (Position >= Line.Len())
			{
				break;
			}
			FString Token;
			if (Line[Position] == '"')
			{
				const int32 Close = Line.Find("\"", ESearchCase::CaseSensitive, ESearchDir::FromStart, Position + 1);
				if (Close == INDEX_NONE)
				{
					Tokens.Reset();
					break;
				}
				Token = Line.Mid(Position + 1, Close - Position - 1);
				Position = Close + 1;
			}
			else
			{
				const int32 Start = Position;
				while (Position < Line.Len() && Line[Position] != ' ' && Line[Position] != '\t')
				{
					++Position;
				}
				Token = Line.Mid(Start, Position - Start);
			}
			Tokens.Add(Token);
		}
		if (Tokens.Num() != 2 || Tokens[0].IsEmpty() || Tokens[1].IsEmpty())
		{
			UE_LOG(LogPakFile, Error, "%s(%d): expected \"<source path>\" \"<path in the pak>\"", ResponseFile,
				LineIndex + 1);
			return false;
		}
		FPakInputPair& Pair = OutPairs.AddDefaulted_GetRef();
		Pair.Source = Tokens[0];
		Pair.Dest = Tokens[1];
	}
	return true;
}
