#pragma once

#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "HAL/FileManager.h"

/**
 * Whole-file load / save helpers (UE: FFileHelper). Text is UTF-8 (TCHAR is UTF-8): loading strips a UTF-8 BOM and
 * converts UTF-16 files that start with a BOM; saving writes UTF-8.
 *
 * Leon: saves write a temporary file next to the target and rename it over the target, so a failed save never leaves
 * a half-written file behind (UE writes in place).
 */
struct CORE_API FFileHelper
{
	/** How SaveStringToFile encodes the text (UE: EEncodingOptions). Leon always writes UTF-8. */
	enum class EEncodingOptions
	{
		AutoDetect,
		ForceAnsi,
		ForceUnicode,
		ForceUTF8,
		ForceUTF8WithoutBOM
	};

	/** Reads the whole file (UE: LoadFileToArray). Flags are EFileRead values. */
	static bool LoadFileToArray(TArray<uint8>& Result, const TCHAR* Filename, uint32 Flags = 0);

	/** Reads a text file (UE: LoadFileToString). */
	static bool LoadFileToString(FString& Result, const TCHAR* Filename, uint32 ReadFlags = 0);

	/** Reads a text file as lines, without line ends (UE: LoadFileToStringArray). */
	static bool LoadFileToStringArray(TArray<FString>& Result, const TCHAR* Filename);

	/** Text from raw file bytes, as LoadFileToString decodes them (UE: BufferToString). */
	static void BufferToString(FString& Result, const uint8* Buffer, int32 Size);

	/** Writes the bytes (UE: SaveArrayToFile). WriteFlags are EFileWrite values; FILEWRITE_Append writes in place. */
	static bool SaveArrayToFile(TArrayView<const uint8> Array, const TCHAR* Filename,
		IFileManager* FileManager = &IFileManager::Get(), uint32 WriteFlags = 0);

	/** Writes the text as UTF-8; ForceUTF8 adds a BOM (UE: SaveStringToFile). */
	static bool SaveStringToFile(const FString& String, const TCHAR* Filename,
		EEncodingOptions EncodingOptions = EEncodingOptions::AutoDetect,
		IFileManager* FileManager = &IFileManager::Get(), uint32 WriteFlags = 0);

	/** Writes the lines, each followed by a line end (UE: SaveStringArrayToFile). */
	static bool SaveStringArrayToFile(const TArray<FString>& Lines, const TCHAR* Filename,
		EEncodingOptions EncodingOptions = EEncodingOptions::AutoDetect,
		IFileManager* FileManager = &IFileManager::Get(), uint32 WriteFlags = 0);
};
