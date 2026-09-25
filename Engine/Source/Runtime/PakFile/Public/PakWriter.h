#pragma once

// Writes .lpak files (Leon; UE writes paks in UnrealPak's PakFileUtilities). LeonPak -create uses it, and so do the
// tests on every platform, which build their paks in memory.

#include "CoreMinimal.h"

/** A file to pak: its source on disk and its path in the pak (UE: FPakInputPair). */
struct PAKFILE_API FPakInputPair
{
	/** The file to read, or empty for bytes added with FPakWriter::AddFile. */
	FString Source;
	/** The path in the pak, with its mount point: "../../../Engine/Content/Maps/Entry.lmap". */
	FString Dest;
};

/**
 * Builds a pak: the entries' bytes, then the index, then the FPakInfo footer (IPlatformFilePak.h). The output depends
 * only on the files (deterministic, like the packages): the data goes in path order, lowercased, the index is sorted by
 * path hash, and nothing records a time or the machine. The mount point is the longest folder every Dest starts with
 * (UnrealPak's rule), and each entry keeps the rest of its path.
 */
class PAKFILE_API FPakWriter
{
public:
	/** Alignment: every entry's data starts at a multiple of it (2048 for a CD's sectors; 0 or 1: packed). */
	explicit FPakWriter(int64 InAlignment = 0);

	/** Adds a file's bytes under its path in the pak (with the mount point). */
	void AddFile(const FString& Dest, TArray<uint8>&& Data);

	/** Reads Source and adds it; false (logged) when it cannot be read. */
	bool AddFileFromDisk(const FPakInputPair& Pair);

	[[nodiscard]] int32 GetNumFiles() const
	{
		return Files.Num();
	}

	/** Writes the pak into OutPak; false (logged) for two files with the same path or no file at all. */
	bool Finalize(TArray<uint8>& OutPak) const;

	/** Finalize into a file (written whole, then moved into place). */
	bool WriteToFile(const TCHAR* Filename) const;

	/** The longest folder every path starts with, ending in '/' ("../../../"); empty when they share none. */
	static FString ComputeMountPoint(const TArray<FString>& Dests);

	/**
	 * Reads a response file (UnrealPak's -create=): one file per line, `"<source path>" "<path in the pak>"`; blank
	 * lines and lines starting with ';' or '#' are skipped. False (logged) for a malformed line.
	 */
	static bool ReadResponseFile(const TCHAR* ResponseFile, TArray<FPakInputPair>& OutPairs);

private:
	struct FFile
	{
		FString Dest;
		TArray<uint8> Data;
	};

	int64 Alignment;
	TArray<FFile> Files;
};
