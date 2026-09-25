#include "Misc/Crc.h"

namespace
{
	/** Table of the reflected polynomial 0xEDB88320 (== 0x04C11DB7 reversed), built on first use. */
	const uint32* GetCrcTable()
	{
		static uint32 Table[256];
		static bool bBuilt = false;
		if (!bBuilt)
		{
			for (uint32 Index = 0; Index < 256; ++Index)
			{
				uint32 Value = Index;
				for (int32 Bit = 0; Bit < 8; ++Bit)
				{
					Value = (Value & 1) ? (0xEDB88320u ^ (Value >> 1)) : (Value >> 1);
				}
				Table[Index] = Value;
			}
			bBuilt = true;
		}
		return Table;
	}
} // namespace

uint32 FCrc::MemCrc32(const void* InData, int32 Length, uint32 CRC)
{
	const uint32* Table = GetCrcTable();
	const uint8* Data = static_cast<const uint8*>(InData);
	CRC = ~CRC;
	for (int32 Index = 0; Index < Length; ++Index)
	{
		CRC = (CRC >> 8) ^ Table[(CRC ^ Data[Index]) & 0xFF];
	}
	return ~CRC;
}

uint32 FCrc::StrCrc32(const TCHAR* Data, uint32 CRC)
{
	const uint32* Table = GetCrcTable();
	CRC = ~CRC;
	for (; *Data; ++Data)
	{
		CRC = (CRC >> 8) ^ Table[(CRC ^ uint8(*Data)) & 0xFF];
	}
	return ~CRC;
}

uint32 FCrc::Strihash_DEPRECATED(const TCHAR* Data)
{
	// FNV-1a over the lower-cased characters.
	uint32 Hash = 2166136261u;
	for (; *Data; ++Data)
	{
		Hash = (Hash ^ uint8(FChar::ToLower(*Data))) * 16777619u;
	}
	return Hash;
}

uint32 FCrc::Strihash_DEPRECATED(int32 Length, const TCHAR* Data)
{
	uint32 Hash = 2166136261u;
	for (int32 Index = 0; Index < Length; ++Index)
	{
		Hash = (Hash ^ uint8(FChar::ToLower(Data[Index]))) * 16777619u;
	}
	return Hash;
}
