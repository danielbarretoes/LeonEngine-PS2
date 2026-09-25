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
};

typedef TChar<TCHAR> FChar;
typedef TChar<ANSICHAR> FCharAnsi;
