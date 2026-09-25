#include "Misc/Guid.h"

#include "HAL/PlatformMisc.h"
#include "Misc/Char.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"

FString FGuid::ToString(EGuidFormats Format) const
{
	switch (Format)
	{
		case EGuidFormats::DigitsWithHyphens:
			return FString::Printf("%08X-%04X-%04X-%04X-%04X%08X", A, B >> 16, B & 0xFFFF, C >> 16, C & 0xFFFF, D);

		case EGuidFormats::DigitsWithHyphensInBraces:
			return FString::Printf("{%08X-%04X-%04X-%04X-%04X%08X}", A, B >> 16, B & 0xFFFF, C >> 16, C & 0xFFFF, D);

		case EGuidFormats::DigitsWithHyphensInParentheses:
			return FString::Printf("(%08X-%04X-%04X-%04X-%04X%08X)", A, B >> 16, B & 0xFFFF, C >> 16, C & 0xFFFF, D);

		case EGuidFormats::HexValuesInBraces:
			return FString::Printf("{0x%08X,0x%04X,0x%04X,{0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X}}",
				A, B >> 16, B & 0xFFFF, C >> 24, (C >> 16) & 0xFF, (C >> 8) & 0xFF, C & 0XFF, D >> 24, (D >> 16) & 0XFF,
				(D >> 8) & 0XFF, D & 0XFF);

		case EGuidFormats::UniqueObjectGuid:
			return FString::Printf("%08X-%08X-%08X-%08X", A, B, C, D);

		default:
			return FString::Printf("%08X%08X%08X%08X", A, B, C, D);
	}
}

FGuid FGuid::NewGuid()
{
	FGuid Result(0, 0, 0, 0);
	FPlatformMisc::CreateGuid(Result);
	return Result;
}

FGuid FGuid::NewDeterministicGuid(const FString& ObjectPath, uint64 Seed)
{
	FMD5 Md5;
	uint8 SeedBytes[8];
	for (int32 Index = 0; Index < 8; ++Index)
	{
		SeedBytes[Index] = uint8(Seed >> (Index * 8));
	}
	Md5.Update(SeedBytes, sizeof(SeedBytes));
	Md5.Update(reinterpret_cast<const uint8*>(*ObjectPath), uint64(ObjectPath.Len()));

	uint8 Digest[16];
	Md5.Final(Digest);

	auto Word = [&Digest](int32 Offset)
	{
		return (uint32(Digest[Offset]) << 24) | (uint32(Digest[Offset + 1]) << 16) | (uint32(Digest[Offset + 2]) << 8) |
			uint32(Digest[Offset + 3]);
	};
	return FGuid(Word(0), Word(4), Word(8), Word(12));
}

bool FGuid::Parse(const FString& GuidString, FGuid& OutGuid)
{
	if (GuidString.Len() == 32)
	{
		return ParseExact(GuidString, EGuidFormats::Digits, OutGuid);
	}
	if (GuidString.Len() == 36)
	{
		return ParseExact(GuidString, EGuidFormats::DigitsWithHyphens, OutGuid);
	}
	if (GuidString.Len() == 38)
	{
		if (GuidString.StartsWith("{"))
		{
			return ParseExact(GuidString, EGuidFormats::DigitsWithHyphensInBraces, OutGuid);
		}
		return ParseExact(GuidString, EGuidFormats::DigitsWithHyphensInParentheses, OutGuid);
	}
	if (GuidString.Len() == 68)
	{
		return ParseExact(GuidString, EGuidFormats::HexValuesInBraces, OutGuid);
	}
	if (GuidString.Len() == 35)
	{
		return ParseExact(GuidString, EGuidFormats::UniqueObjectGuid, OutGuid);
	}
	return false;
}

namespace
{
	/** The four hyphen-separated groups of "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX" starting at Offset. */
	bool ParseHyphenated(const FString& GuidString, int32 Offset, FString& OutDigits)
	{
		if (GuidString[Offset + 8] != '-' || GuidString[Offset + 13] != '-' || GuidString[Offset + 18] != '-' ||
			GuidString[Offset + 23] != '-')
		{
			return false;
		}
		OutDigits += GuidString.Mid(Offset + 0, 8);
		OutDigits += GuidString.Mid(Offset + 9, 4);
		OutDigits += GuidString.Mid(Offset + 14, 4);
		OutDigits += GuidString.Mid(Offset + 19, 4);
		OutDigits += GuidString.Mid(Offset + 24, 12);
		return true;
	}
} // namespace

bool FGuid::ParseExact(const FString& GuidString, EGuidFormats Format, FGuid& OutGuid)
{
	FString NormalizedGuidString;
	NormalizedGuidString.Reserve(32);

	switch (Format)
	{
		case EGuidFormats::Digits:
			NormalizedGuidString = GuidString;
			break;

		case EGuidFormats::DigitsWithHyphens:
			if (GuidString.Len() != 36 || !ParseHyphenated(GuidString, 0, NormalizedGuidString))
			{
				return false;
			}
			break;

		case EGuidFormats::DigitsWithHyphensInBraces:
			if (GuidString.Len() != 38 || GuidString[0] != '{' || GuidString[37] != '}' ||
				!ParseHyphenated(GuidString, 1, NormalizedGuidString))
			{
				return false;
			}
			break;

		case EGuidFormats::DigitsWithHyphensInParentheses:
			if (GuidString.Len() != 38 || GuidString[0] != '(' || GuidString[37] != ')' ||
				!ParseHyphenated(GuidString, 1, NormalizedGuidString))
			{
				return false;
			}
			break;

		case EGuidFormats::HexValuesInBraces:
		{
			// {0xAAAAAAAA,0xBBBB,0xCCCC,{0xDD,0xDD,0xDD,0xDD,0xDD,0xDD,0xDD,0xDD}}
			if (GuidString.Len() != 68 || GuidString[0] != '{' || GuidString[11] != ',' || GuidString[18] != ',' ||
				GuidString[25] != ',' || GuidString[26] != '{' || GuidString[66] != '}' || GuidString[67] != '}')
			{
				return false;
			}
			const int32 Prefixes[] = {1, 12, 19, 27, 32, 37, 42, 47, 52, 57, 62};
			for (const int32 Prefix : Prefixes)
			{
				if (GuidString[Prefix] != '0' || GuidString[Prefix + 1] != 'x')
				{
					return false;
				}
			}
			for (int32 Byte = 0; Byte < 7; ++Byte)
			{
				if (GuidString[31 + Byte * 5] != ',')
				{
					return false;
				}
			}
			NormalizedGuidString += GuidString.Mid(3, 8);
			NormalizedGuidString += GuidString.Mid(14, 4);
			NormalizedGuidString += GuidString.Mid(21, 4);
			for (int32 Byte = 0; Byte < 8; ++Byte)
			{
				NormalizedGuidString += GuidString.Mid(29 + Byte * 5, 2);
			}
			break;
		}

		case EGuidFormats::UniqueObjectGuid:
			if (GuidString.Len() != 35 || GuidString[8] != '-' || GuidString[17] != '-' || GuidString[26] != '-')
			{
				return false;
			}
			NormalizedGuidString += GuidString.Mid(0, 8);
			NormalizedGuidString += GuidString.Mid(9, 8);
			NormalizedGuidString += GuidString.Mid(18, 8);
			NormalizedGuidString += GuidString.Mid(27, 8);
			break;

		default:
			return false;
	}

	if (NormalizedGuidString.Len() != 32)
	{
		return false;
	}
	for (int32 Index = 0; Index < NormalizedGuidString.Len(); ++Index)
	{
		if (!FChar::IsHexDigit(NormalizedGuidString[Index]))
		{
			return false;
		}
	}

	OutGuid =
		FGuid(FParse::HexNumber(*NormalizedGuidString.Mid(0, 8)), FParse::HexNumber(*NormalizedGuidString.Mid(8, 8)),
			FParse::HexNumber(*NormalizedGuidString.Mid(16, 8)), FParse::HexNumber(*NormalizedGuidString.Mid(24, 8)));
	return true;
}
