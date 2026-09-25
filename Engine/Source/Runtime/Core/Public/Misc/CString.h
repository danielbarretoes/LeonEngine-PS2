#pragma once

#include "CoreTypes.h"
#include "Misc/Char.h"

#include <cstdarg>

/** Case sensitivity of string searches (UE: ESearchCase). */
namespace ESearchCase
{
	enum Type
	{
		CaseSensitive,
		IgnoreCase,
	};
} // namespace ESearchCase

/** Direction of string searches (UE: ESearchDir). */
namespace ESearchDir
{
	enum Type
	{
		FromStart,
		FromEnd,
	};
} // namespace ESearchDir

/** C string functions on TCHAR (UE: TCString / FCString). Case-insensitive functions fold ASCII only. */
template <typename T>
struct TCString
{
	typedef T CharType;

	static FORCEINLINE int32 Strlen(const CharType* String)
	{
		const CharType* End = String;
		while (*End)
		{
			++End;
		}
		return static_cast<int32>(End - String);
	}

	/** Length, stopping at StringSize characters. */
	static FORCEINLINE int32 Strnlen(const CharType* String, SIZE_T StringSize)
	{
		SIZE_T Length = 0;
		while (Length < StringSize && String[Length])
		{
			++Length;
		}
		return static_cast<int32>(Length);
	}

	static int32 Strcmp(const CharType* String1, const CharType* String2)
	{
		for (;; ++String1, ++String2)
		{
			const int32 Diff = int32(uint8(*String1)) - int32(uint8(*String2));
			if (Diff != 0 || *String1 == 0)
			{
				return Diff;
			}
		}
	}

	static int32 Strncmp(const CharType* String1, const CharType* String2, SIZE_T Count)
	{
		for (; Count; --Count, ++String1, ++String2)
		{
			const int32 Diff = int32(uint8(*String1)) - int32(uint8(*String2));
			if (Diff != 0 || *String1 == 0)
			{
				return Diff;
			}
		}
		return 0;
	}

	static int32 Stricmp(const CharType* String1, const CharType* String2)
	{
		for (;; ++String1, ++String2)
		{
			const int32 Diff =
				int32(uint8(TChar<CharType>::ToLower(*String1))) - int32(uint8(TChar<CharType>::ToLower(*String2)));
			if (Diff != 0 || *String1 == 0)
			{
				return Diff;
			}
		}
	}

	static int32 Strnicmp(const CharType* String1, const CharType* String2, SIZE_T Count)
	{
		for (; Count; --Count, ++String1, ++String2)
		{
			const int32 Diff =
				int32(uint8(TChar<CharType>::ToLower(*String1))) - int32(uint8(TChar<CharType>::ToLower(*String2)));
			if (Diff != 0 || *String1 == 0)
			{
				return Diff;
			}
		}
		return 0;
	}

	/** Copies at most DestCount - 1 characters and terminates (never overflows Dest). */
	static CharType* Strncpy(CharType* Dest, const CharType* Src, SIZE_T DestCount)
	{
		if (DestCount == 0)
		{
			return Dest;
		}
		SIZE_T Index = 0;
		for (; Index + 1 < DestCount && Src[Index]; ++Index)
		{
			Dest[Index] = Src[Index];
		}
		Dest[Index] = 0;
		return Dest;
	}

	/** Strcpy into an array; truncates to the array size. */
	template <SIZE_T DestCount>
	static FORCEINLINE CharType* Strcpy(CharType (&Dest)[DestCount], const CharType* Src)
	{
		return Strncpy(Dest, Src, DestCount);
	}

	static FORCEINLINE CharType* Strcpy(CharType* Dest, SIZE_T DestCount, const CharType* Src)
	{
		return Strncpy(Dest, Src, DestCount);
	}

	/** Appends Src; the result never exceeds DestCount characters including the terminator. */
	static CharType* Strncat(CharType* Dest, const CharType* Src, SIZE_T DestCount)
	{
		const int32 DestLen = Strnlen(Dest, DestCount);
		if (SIZE_T(DestLen) + 1 < DestCount)
		{
			Strncpy(Dest + DestLen, Src, DestCount - DestLen);
		}
		return Dest;
	}

	template <SIZE_T DestCount>
	static FORCEINLINE CharType* Strcat(CharType (&Dest)[DestCount], const CharType* Src)
	{
		return Strncat(Dest, Src, DestCount);
	}

	static FORCEINLINE CharType* Strcat(CharType* Dest, SIZE_T DestCount, const CharType* Src)
	{
		return Strncat(Dest, Src, DestCount);
	}

	/** Upper-cases in place (ASCII). */
	static CharType* Strupr(CharType* Dest)
	{
		for (CharType* Char = Dest; *Char; ++Char)
		{
			*Char = TChar<CharType>::ToUpper(*Char);
		}
		return Dest;
	}

	static const CharType* Strchr(const CharType* String, CharType Char)
	{
		for (; *String != Char; ++String)
		{
			if (*String == 0)
			{
				return nullptr;
			}
		}
		return String;
	}

	static const CharType* Strrchr(const CharType* String, CharType Char)
	{
		const CharType* Last = nullptr;
		for (;; ++String)
		{
			if (*String == Char)
			{
				Last = String;
			}
			if (*String == 0)
			{
				return Last;
			}
		}
	}

	static const CharType* Strstr(const CharType* String, const CharType* Find)
	{
		if (*Find == 0)
		{
			return String;
		}
		const int32 FindLen = Strlen(Find);
		for (; *String; ++String)
		{
			if (Strncmp(String, Find, FindLen) == 0)
			{
				return String;
			}
		}
		return nullptr;
	}

	static const CharType* Stristr(const CharType* String, const CharType* Find)
	{
		if (*Find == 0)
		{
			return String;
		}
		const int32 FindLen = Strlen(Find);
		for (; *String; ++String)
		{
			if (Strnicmp(String, Find, FindLen) == 0)
			{
				return String;
			}
		}
		return nullptr;
	}

	/**
	 * Case-insensitive search for Find that starts a word: the character before the match must not be A-Z / 0-9.
	 * With bSkipQuotedChars, text between double quotes is ignored (UE: FCString::Strifind).
	 */
	static const CharType* Strifind(const CharType* Str, const CharType* Find, bool bSkipQuotedChars = false)
	{
		if (Find == nullptr || Str == nullptr)
		{
			return nullptr;
		}

		bool bAlnum = false;
		const CharType First = (*Find < 'a' || *Find > 'z') ? (*Find) : CharType(*Find + 'A' - 'a');
		const int32 Length = Strlen(Find++) - 1;
		CharType C = *Str++;
		bool bInQuotedStr = false;
		while (C)
		{
			if (!bInQuotedStr && C >= 'a' && C <= 'z')
			{
				C = CharType(C + 'A' - 'a');
			}
			if (!bInQuotedStr && !bAlnum && C == First && !Strnicmp(Str, Find, Length))
			{
				return Str - 1;
			}
			if (bSkipQuotedChars && C == '"')
			{
				bInQuotedStr = !bInQuotedStr;
			}
			bAlnum = (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9');
			C = *Str++;
		}
		return nullptr;
	}

	static FORCEINLINE CharType* Strchr(CharType* String, CharType Char)
	{
		return const_cast<CharType*>(Strchr(const_cast<const CharType*>(String), Char));
	}
	static FORCEINLINE CharType* Strrchr(CharType* String, CharType Char)
	{
		return const_cast<CharType*>(Strrchr(const_cast<const CharType*>(String), Char));
	}
	static FORCEINLINE CharType* Strstr(CharType* String, const CharType* Find)
	{
		return const_cast<CharType*>(Strstr(const_cast<const CharType*>(String), Find));
	}
	static FORCEINLINE CharType* Stristr(CharType* String, const CharType* Find)
	{
		return const_cast<CharType*>(Stristr(const_cast<const CharType*>(String), Find));
	}

	/** Length of the initial run of characters found in Mask. */
	static int32 Strspn(const CharType* String, const CharType* Mask)
	{
		const CharType* Start = String;
		while (*String && Strchr(Mask, *String))
		{
			++String;
		}
		return static_cast<int32>(String - Start);
	}

	/** Length of the initial run of characters not found in Mask. */
	static int32 Strcspn(const CharType* String, const CharType* Mask)
	{
		const CharType* Start = String;
		while (*String && !Strchr(Mask, *String))
		{
			++String;
		}
		return static_cast<int32>(String - Start);
	}

	static int32 Atoi(const CharType* String);
	static int64 Atoi64(const CharType* String);
	static float Atof(const CharType* String);
	static double Atod(const CharType* String);
	static int32 Strtoi(const CharType* Start, CharType** End, int32 Base);
	static int64 Strtoi64(const CharType* Start, CharType** End, int32 Base);
	static uint64 Strtoui64(const CharType* Start, CharType** End, int32 Base);
	static float Strtof(const CharType* Start, CharType** End);
	static double Strtod(const CharType* Start, CharType** End);

	/** "true" / "yes" / "on" / non-zero number (case-insensitive) (UE: FCString::ToBool). */
	static bool ToBool(const CharType* String);

	/** Optional sign, digits, optional '.' and digits (UE: FCString::IsNumeric). */
	static bool IsNumeric(const CharType* Str);

	/** A static string of NumSpaces spaces (at most 255) (UE: FCString::Spc). */
	static const CharType* Spc(int32 NumSpaces);

	/** A static string of NumTabs tabs (at most 255). */
	static const CharType* Tab(int32 NumTabs);

	/** snprintf into Dest; returns the number of characters written, or -1 when truncated (UE: FCString::Snprintf). */
	static int32 Snprintf(CharType* Dest, int32 DestSize, const CharType* Fmt, ...);

	/** vsnprintf; returns the number of characters written, or -1 when truncated (UE: FCString::GetVarArgs). */
	static int32 GetVarArgs(CharType* Dest, SIZE_T DestSize, const CharType* Fmt, va_list ArgPtr);

	/** Sprintf into an array (truncates). */
	template <SIZE_T DestCount>
	static int32 Sprintf(CharType (&Dest)[DestCount], const CharType* Fmt, ...)
	{
		va_list Args;
		va_start(Args, Fmt);
		const int32 Result = GetVarArgs(Dest, DestCount, Fmt, Args);
		va_end(Args);
		return Result;
	}
};

typedef TCString<TCHAR> FCString;
typedef TCString<ANSICHAR> FCStringAnsi;
