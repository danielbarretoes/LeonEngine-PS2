#include "Containers/StringConv.h"

#include "HAL/UnrealMemory.h"

namespace
{
	constexpr uint32 ReplacementCharacter = 0xFFFD;

	/** Decodes one code point from UTF-8; advances Index past it. */
	uint32 DecodeUtf8(const TCHAR* Source, int32 SourceLength, int32& Index)
	{
		const uint8 Lead = uint8(Source[Index++]);
		if (Lead < 0x80)
		{
			return Lead;
		}

		int32 Continuations = 0;
		uint32 CodePoint = 0;
		uint32 MinValue = 0;
		if ((Lead & 0xE0) == 0xC0)
		{
			Continuations = 1;
			CodePoint = Lead & 0x1F;
			MinValue = 0x80;
		}
		else if ((Lead & 0xF0) == 0xE0)
		{
			Continuations = 2;
			CodePoint = Lead & 0x0F;
			MinValue = 0x800;
		}
		else if ((Lead & 0xF8) == 0xF0)
		{
			Continuations = 3;
			CodePoint = Lead & 0x07;
			MinValue = 0x10000;
		}
		else
		{
			return ReplacementCharacter;
		}

		for (int32 Count = 0; Count < Continuations; ++Count)
		{
			if (Index >= SourceLength || (uint8(Source[Index]) & 0xC0) != 0x80)
			{
				return ReplacementCharacter;
			}
			CodePoint = (CodePoint << 6) | (uint8(Source[Index++]) & 0x3F);
		}

		// Overlong encodings, surrogates and out-of-range values are invalid.
		if (CodePoint < MinValue || CodePoint > 0x10FFFF || (CodePoint >= 0xD800 && CodePoint <= 0xDFFF))
		{
			return ReplacementCharacter;
		}
		return CodePoint;
	}

	/** Number of WIDECHARs needed for a code point. */
	FORCEINLINE int32 WideUnits(uint32 CodePoint)
	{
		return (sizeof(WIDECHAR) == 2 && CodePoint >= 0x10000) ? 2 : 1;
	}

	FORCEINLINE void EncodeWide(uint32 CodePoint, WIDECHAR*& Out)
	{
		if (sizeof(WIDECHAR) == 2 && CodePoint >= 0x10000)
		{
			CodePoint -= 0x10000;
			*Out++ = WIDECHAR(0xD800 + (CodePoint >> 10));
			*Out++ = WIDECHAR(0xDC00 + (CodePoint & 0x3FF));
		}
		else
		{
			*Out++ = WIDECHAR(CodePoint);
		}
	}

	/** Decodes one code point from a wide string (UTF-16 with surrogates, or UTF-32). */
	uint32 DecodeWide(const WIDECHAR* Source, int32 SourceLength, int32& Index)
	{
		uint32 CodePoint = uint32(Source[Index++]);
		if (sizeof(WIDECHAR) == 2 && CodePoint >= 0xD800 && CodePoint <= 0xDBFF)
		{
			if (Index < SourceLength)
			{
				const uint32 Low = uint32(Source[Index]);
				if (Low >= 0xDC00 && Low <= 0xDFFF)
				{
					++Index;
					return 0x10000 + ((CodePoint - 0xD800) << 10) + (Low - 0xDC00);
				}
			}
			return ReplacementCharacter;
		}
		if ((CodePoint >= 0xD800 && CodePoint <= 0xDFFF) || CodePoint > 0x10FFFF)
		{
			return ReplacementCharacter;
		}
		return CodePoint;
	}

	FORCEINLINE int32 Utf8Units(uint32 CodePoint)
	{
		return CodePoint < 0x80 ? 1 : (CodePoint < 0x800 ? 2 : (CodePoint < 0x10000 ? 3 : 4));
	}

	FORCEINLINE void EncodeUtf8(uint32 CodePoint, TCHAR*& Out)
	{
		if (CodePoint < 0x80)
		{
			*Out++ = TCHAR(CodePoint);
		}
		else if (CodePoint < 0x800)
		{
			*Out++ = TCHAR(0xC0 | (CodePoint >> 6));
			*Out++ = TCHAR(0x80 | (CodePoint & 0x3F));
		}
		else if (CodePoint < 0x10000)
		{
			*Out++ = TCHAR(0xE0 | (CodePoint >> 12));
			*Out++ = TCHAR(0x80 | ((CodePoint >> 6) & 0x3F));
			*Out++ = TCHAR(0x80 | (CodePoint & 0x3F));
		}
		else
		{
			*Out++ = TCHAR(0xF0 | (CodePoint >> 18));
			*Out++ = TCHAR(0x80 | ((CodePoint >> 12) & 0x3F));
			*Out++ = TCHAR(0x80 | ((CodePoint >> 6) & 0x3F));
			*Out++ = TCHAR(0x80 | (CodePoint & 0x3F));
		}
	}

	template <typename CharType>
	int32 LengthOf(const CharType* String)
	{
		int32 Length = 0;
		if (String)
		{
			while (String[Length])
			{
				++Length;
			}
		}
		return Length;
	}
} // namespace

FTCHARToWide::FTCHARToWide(const TCHAR* Source)
{
	Convert(Source, LengthOf(Source));
}

FTCHARToWide::FTCHARToWide(const TCHAR* Source, int32 SourceLength)
{
	Convert(Source, SourceLength);
}

FTCHARToWide::~FTCHARToWide()
{
	if (Buffer != InlineBuffer)
	{
		FMemory::Free(Buffer);
	}
}

void FTCHARToWide::Convert(const TCHAR* Source, int32 SourceLength)
{
	// First pass: size.
	int32 Needed = 0;
	for (int32 Index = 0; Index < SourceLength;)
	{
		Needed += WideUnits(DecodeUtf8(Source, SourceLength, Index));
	}
	if (Needed + 1 > int32(sizeof(InlineBuffer) / sizeof(WIDECHAR)))
	{
		Buffer = static_cast<WIDECHAR*>(FMemory::Malloc(SIZE_T(Needed + 1) * sizeof(WIDECHAR)));
	}

	// Second pass: encode.
	WIDECHAR* Out = Buffer;
	for (int32 Index = 0; Index < SourceLength;)
	{
		EncodeWide(DecodeUtf8(Source, SourceLength, Index), Out);
	}
	*Out = 0;
	ConvertedLength = Needed;
}

FWideToTCHAR::FWideToTCHAR(const WIDECHAR* Source)
{
	Convert(Source, LengthOf(Source));
}

FWideToTCHAR::FWideToTCHAR(const WIDECHAR* Source, int32 SourceLength)
{
	Convert(Source, SourceLength);
}

FWideToTCHAR::~FWideToTCHAR()
{
	if (Buffer != InlineBuffer)
	{
		FMemory::Free(Buffer);
	}
}

void FWideToTCHAR::Convert(const WIDECHAR* Source, int32 SourceLength)
{
	int32 Needed = 0;
	for (int32 Index = 0; Index < SourceLength;)
	{
		Needed += Utf8Units(DecodeWide(Source, SourceLength, Index));
	}
	if (Needed + 1 > int32(sizeof(InlineBuffer)))
	{
		Buffer = static_cast<TCHAR*>(FMemory::Malloc(SIZE_T(Needed + 1)));
	}

	TCHAR* Out = Buffer;
	for (int32 Index = 0; Index < SourceLength;)
	{
		EncodeUtf8(DecodeWide(Source, SourceLength, Index), Out);
	}
	*Out = 0;
	ConvertedLength = Needed;
}
