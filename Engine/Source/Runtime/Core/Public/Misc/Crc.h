#pragma once

#include "CoreTypes.h"
#include "Misc/Char.h"

/** CRC-32 and string hashes (UE: FCrc). */
struct CORE_API FCrc
{
	/** CRC-32 (polynomial 0x04C11DB7, reflected) of a block of memory (UE: MemCrc32). */
	static uint32 MemCrc32(const void* Data, int32 Length, uint32 CRC = 0);

	/** CRC-32 of a null-terminated string, case-sensitive (UE: StrCrc32). */
	static uint32 StrCrc32(const TCHAR* Data, uint32 CRC = 0);

	/** Case-insensitive hash of a null-terminated string; FString / FName hashing (UE: Strihash_DEPRECATED). */
	static uint32 Strihash_DEPRECATED(const TCHAR* Data);

	/** Case-insensitive hash of Length characters. */
	static uint32 Strihash_DEPRECATED(int32 Length, const TCHAR* Data);
};
