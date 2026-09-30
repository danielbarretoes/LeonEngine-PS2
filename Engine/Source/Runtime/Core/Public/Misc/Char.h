#pragma once

#include "CoreTypes.h"

/**
 * Character classification and conversion (UE: TChar / FChar). TCHAR is UTF-8, so only ASCII is classified; bytes of
 * multi-byte sequences are neither letters nor digits.
 */
template <typename CharType>
struct TChar
{
	static constexpr FORCEINLINE CharType ToUpper(CharType Char)
	{
		return (Char >= 'a' && Char <= 'z') ? CharType(Char - ('a' - 'A')) : Char;
	}

	static constexpr FORCEINLINE CharType ToLower(CharType Char)
	{
		return (Char >= 'A' && Char <= 'Z') ? CharType(Char + ('a' - 'A')) : Char;
	}

	static constexpr FORCEINLINE bool IsUpper(CharType Char)
	{
		return Char >= 'A' && Char <= 'Z';
	}

	static constexpr FORCEINLINE bool IsLower(CharType Char)
	{
		return Char >= 'a' && Char <= 'z';
	}

	static constexpr FORCEINLINE bool IsAlpha(CharType Char)
	{
		return IsUpper(Char) || IsLower(Char);
	}

	static constexpr FORCEINLINE bool IsDigit(CharType Char)
	{
		return Char >= '0' && Char <= '9';
	}

	static constexpr FORCEINLINE bool IsAlnum(CharType Char)
	{
		return IsAlpha(Char) || IsDigit(Char);
	}

	static constexpr FORCEINLINE bool IsHexDigit(CharType Char)
	{
		return IsDigit(Char) || (Char >= 'a' && Char <= 'f') || (Char >= 'A' && Char <= 'F');
	}

	static constexpr FORCEINLINE bool IsOctDigit(CharType Char)
	{
		return Char >= '0' && Char <= '7';
	}

	static constexpr FORCEINLINE bool IsWhitespace(CharType Char)
	{
		return Char == ' ' || Char == '\t' || Char == '\n' || Char == '\r' || Char == '\v' || Char == '\f';
	}

	static constexpr FORCEINLINE bool IsLinebreak(CharType Char)
	{
		return Char == '\n' || Char == '\r' || Char == '\v' || Char == '\f';
	}

	static constexpr FORCEINLINE bool IsPrint(CharType Char)
	{
		return Char >= 0x20 && Char < 0x7f;
	}

	static constexpr FORCEINLINE bool IsGraph(CharType Char)
	{
		return Char > 0x20 && Char < 0x7f;
	}

	static constexpr FORCEINLINE bool IsPunct(CharType Char)
	{
		return IsGraph(Char) && !IsAlnum(Char);
	}

	/** Letters, digits and underscore (C identifier characters). */
	static constexpr FORCEINLINE bool IsIdentifier(CharType Char)
	{
		return IsAlnum(Char) || Char == '_';
	}

	static constexpr FORCEINLINE bool IsUnderscore(CharType Char)
	{
		return Char == '_';
	}

	/** '7' -> 7 (only valid for digits). */
	static constexpr FORCEINLINE int32 ConvertCharDigitToInt(CharType Char)
	{
		return static_cast<int32>(Char) - static_cast<int32>('0');
	}

	/**
	 * The next code point of UTF-8 text at Cursor, which moves past it (Leon: TCHAR is a byte; the fonts and the GS's
	 * debug text read it). One-, two- and three-byte sequences decode; a byte that does not start a valid sequence
	 * (a stray continuation, a truncated or overlong sequence) is taken as its Latin-1 character, so Latin-1 text reads
	 * too. Cursor must be before End.
	 */
	static uint32 DecodeCodePoint(const CharType*& Cursor, const CharType* End)
	{
		const uint8 Lead = static_cast<uint8>(*Cursor++);
		if (Lead < 0x80)
		{
			return Lead;
		}
		int32 Continuations = 0;
		uint32 CodePoint = 0;
		if ((Lead & 0xe0) == 0xc0)
		{
			Continuations = 1;
			CodePoint = Lead & 0x1fu;
		}
		else if ((Lead & 0xf0) == 0xe0)
		{
			Continuations = 2;
			CodePoint = Lead & 0x0fu;
		}
		else
		{
			return Lead;
		}
		if (End - Cursor < Continuations)
		{
			return Lead;
		}
		for (int32 Index = 0; Index < Continuations; ++Index)
		{
			const uint8 Next = static_cast<uint8>(Cursor[Index]);
			if ((Next & 0xc0) != 0x80)
			{
				return Lead;
			}
			CodePoint = (CodePoint << 6) | (Next & 0x3fu);
		}
		// An overlong sequence is not UTF-8: its lead byte is Latin-1.
		if (CodePoint < (Continuations == 1 ? 0x80u : 0x800u))
		{
			return Lead;
		}
		Cursor += Continuations;
		return CodePoint;
	}
};

typedef TChar<TCHAR> FChar;
typedef TChar<ANSICHAR> FCharAnsi;
