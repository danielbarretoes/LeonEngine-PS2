#pragma once

#include "Containers/Array.h"
#include "Containers/ContainersFwd.h"
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CString.h"
#include "Misc/Crc.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"

#include <cstdarg>
#include <type_traits>

/**
 * Dynamic string of TCHARs (UTF-8) with a null terminator (UE: FString). operator== / < and GetTypeHash ignore case
 * (UE); Equals / Compare take an explicit ESearchCase.
 */
class CORE_API FString
{
public:
	using AllocatorType = TSizedDefaultAllocator<32>;
	using ElementType = TCHAR;

private:
	typedef TArray<TCHAR, AllocatorType> DataType;
	DataType Data;

public:
	FString() = default;
	FString(FString&&) = default;
	FString(const FString&) = default;
	FString& operator=(FString&&) = default;
	FString& operator=(const FString&) = default;

	/** Copy with extra slack for later appends. */
	FORCEINLINE FString(const FString& Other, int32 ExtraSlack)
		: Data(Other.Data, ExtraSlack + ((Other.Data.Num() || !ExtraSlack) ? 0 : 1))
	{
	}

	/** Null-terminated TCHAR string; nullptr makes an empty string. */
	FString(const TCHAR* Str);

	/** InCount characters (need not be null-terminated). */
	explicit FString(int32 InCount, const TCHAR* InSrc);

	/** Converts a wide string (UTF-16 on Windows, UTF-32 elsewhere) to UTF-8. */
	explicit FString(const WIDECHAR* Str);

	FString& operator=(const TCHAR* Other);

	// Access ---------------------------------------------------------------------------------------------------------

	FORCEINLINE TCHAR& operator[](int32 Index)
	{
		checkf(IsValidIndex(Index), "String index out of bounds: Index %d from a string with a length of %d", Index,
			Len());
		return Data.GetData()[Index];
	}
	FORCEINLINE const TCHAR& operator[](int32 Index) const
	{
		checkf(IsValidIndex(Index), "String index out of bounds: Index %d from a string with a length of %d", Index,
			Len());
		return Data.GetData()[Index];
	}

	/** The characters; "" for an empty string (never nullptr). */
	FORCEINLINE const TCHAR* operator*() const
	{
		return Data.Num() ? Data.GetData() : TEXT("");
	}

	FORCEINLINE DataType& GetCharArray()
	{
		return Data;
	}
	FORCEINLINE const DataType& GetCharArray() const
	{
		return Data;
	}

	FORCEINLINE int32 Len() const
	{
		return Data.Num() ? Data.Num() - 1 : 0;
	}

	FORCEINLINE bool IsEmpty() const
	{
		return Data.Num() <= 1;
	}

	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return Index >= 0 && Index < Len();
	}

	FORCEINLINE SIZE_T GetAllocatedSize() const
	{
		return Data.GetAllocatedSize();
	}

	FORCEINLINE void CheckInvariants() const
	{
		const int32 Num = Data.Num();
		checkSlow(Num >= 0);
		checkSlow(!Num || !Data.GetData()[Num - 1]);
		checkSlow(Data.Max() >= Num);
		(void)Num;
	}

	/** Empties the string; keeps Slack characters of storage. */
	FORCEINLINE void Empty(int32 Slack = 0)
	{
		Data.Empty(Slack);
	}

	/** Empties the string, keeping (at least) NewReservedSize characters of storage. */
	FORCEINLINE void Reset(int32 NewReservedSize = 0)
	{
		const int32 NewSizeIncludingTerminator = (NewReservedSize > 0) ? (NewReservedSize + 1) : 0;
		Data.Reset(NewSizeIncludingTerminator);
	}

	FORCEINLINE void Shrink()
	{
		Data.Shrink();
	}

	FORCEINLINE void Reserve(int32 CharacterCount)
	{
		checkSlow(CharacterCount >= 0 && CharacterCount < 0x7fffffff);
		if (CharacterCount > 0)
		{
			Data.Reserve(CharacterCount + 1);
		}
	}

	typedef TIndexedContainerIterator<DataType, TCHAR, int32> TIterator;
	typedef TIndexedContainerIterator<const DataType, const TCHAR, int32> TConstIterator;

	TIterator CreateIterator()
	{
		return TIterator(Data);
	}
	TConstIterator CreateConstIterator() const
	{
		return TConstIterator(Data);
	}

	// Ranged-for over the characters (excludes the terminator; lower-case names required by the language).
	FORCEINLINE auto begin()
	{
		return Data.begin();
	}
	FORCEINLINE auto begin() const
	{
		return Data.begin();
	}
	FORCEINLINE auto end()
	{
		auto Result = Data.end();
		if (Data.Num())
		{
			--Result;
		}
		return Result;
	}
	FORCEINLINE auto end() const
	{
		auto Result = Data.end();
		if (Data.Num())
		{
			--Result;
		}
		return Result;
	}

	// Appending ------------------------------------------------------------------------------------------------------

	FString& AppendChar(TCHAR InChar);

	/** Appends Count characters (need not be null-terminated). */
	void AppendChars(const TCHAR* Array, int32 Count);

	FORCEINLINE FString& Append(const TCHAR* Text, int32 Count)
	{
		AppendChars(Text, Count);
		return *this;
	}
	FORCEINLINE FString& Append(const TCHAR* Text)
	{
		AppendChars(Text, Text ? FCString::Strlen(Text) : 0);
		return *this;
	}
	FORCEINLINE FString& Append(const FString& Text)
	{
		AppendChars(Text.Data.GetData(), Text.Len());
		return *this;
	}

	FORCEINLINE FString& operator+=(const TCHAR* Str)
	{
		return Append(Str);
	}
	FORCEINLINE FString& operator+=(const FString& Str)
	{
		return Append(Str);
	}
	FORCEINLINE FString& operator+=(TCHAR InChar)
	{
		return AppendChar(InChar);
	}

	/** Appends a path component with exactly one '/' between (UE: PathAppend / operator/=). */
	void PathAppend(const TCHAR* Str, int32 StrLength);

	FORCEINLINE FString& operator/=(const TCHAR* Str)
	{
		PathAppend(Str, FCString::Strlen(Str));
		return *this;
	}
	FORCEINLINE FString& operator/=(const FString& Str)
	{
		PathAppend(*Str, Str.Len());
		return *this;
	}

	/** Appends printf-formatted text (UE: Appendf). */
	FString& Appendf(const TCHAR* Fmt, ...) LEON_PRINTF_FORMAT(2, 3);

	void AppendInt(int32 InNum);

	FORCEINLINE void InsertAt(int32 Index, TCHAR Character)
	{
		if (Character != 0)
		{
			if (Data.Num() == 0)
			{
				*this += Character;
			}
			else
			{
				Data.Insert(Character, Index);
			}
		}
	}

	void InsertAt(int32 Index, const FString& Characters);

	/** Removes Count characters at Index. */
	FORCEINLINE void RemoveAt(int32 Index, int32 Count = 1, bool bAllowShrinking = true)
	{
		Data.RemoveAt(Index, FMath::Clamp(Count, 0, Len() - Index), bAllowShrinking);
	}

	/** Removes a prefix; true when it was there. */
	bool RemoveFromStart(const TCHAR* InPrefix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);
	bool RemoveFromStart(const FString& InPrefix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);

	/** Removes a suffix; true when it was there. */
	bool RemoveFromEnd(const TCHAR* InSuffix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);
	bool RemoveFromEnd(const FString& InSuffix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);

	// Comparison (operators ignore case, like UE)
	// ----------------------------------------------------------------------

	FORCEINLINE friend bool operator==(const FString& Lhs, const FString& Rhs)
	{
		return Lhs.Equals(Rhs, ESearchCase::IgnoreCase);
	}
	FORCEINLINE friend bool operator==(const FString& Lhs, const TCHAR* Rhs)
	{
		return FCString::Stricmp(*Lhs, Rhs ? Rhs : TEXT("")) == 0;
	}
	FORCEINLINE friend bool operator==(const TCHAR* Lhs, const FString& Rhs)
	{
		return FCString::Stricmp(Lhs ? Lhs : TEXT(""), *Rhs) == 0;
	}
	FORCEINLINE friend bool operator!=(const FString& Lhs, const FString& Rhs)
	{
		return !(Lhs == Rhs);
	}
	FORCEINLINE friend bool operator!=(const FString& Lhs, const TCHAR* Rhs)
	{
		return !(Lhs == Rhs);
	}
	FORCEINLINE friend bool operator!=(const TCHAR* Lhs, const FString& Rhs)
	{
		return !(Lhs == Rhs);
	}
	FORCEINLINE friend bool operator<(const FString& Lhs, const FString& Rhs)
	{
		return FCString::Stricmp(*Lhs, *Rhs) < 0;
	}
	FORCEINLINE friend bool operator<=(const FString& Lhs, const FString& Rhs)
	{
		return FCString::Stricmp(*Lhs, *Rhs) <= 0;
	}
	FORCEINLINE friend bool operator>(const FString& Lhs, const FString& Rhs)
	{
		return FCString::Stricmp(*Lhs, *Rhs) > 0;
	}
	FORCEINLINE friend bool operator>=(const FString& Lhs, const FString& Rhs)
	{
		return FCString::Stricmp(*Lhs, *Rhs) >= 0;
	}

	/** Equality with explicit case handling (default: case-sensitive, like UE). */
	FORCEINLINE bool Equals(const FString& Other, ESearchCase::Type SearchCase = ESearchCase::CaseSensitive) const
	{
		const int32 Num = Data.Num();
		const int32 OtherNum = Other.Data.Num();
		if (Num != OtherNum)
		{
			// Handle special case where FString() == FString("").
			return Num + OtherNum == 1;
		}
		if (Num > 1)
		{
			return SearchCase == ESearchCase::CaseSensitive
				? FCString::Strcmp(Data.GetData(), Other.Data.GetData()) == 0
				: FCString::Stricmp(Data.GetData(), Other.Data.GetData()) == 0;
		}
		return true;
	}

	/** <0, 0, >0 like strcmp (default: case-sensitive). */
	FORCEINLINE int32 Compare(const FString& Other, ESearchCase::Type SearchCase = ESearchCase::CaseSensitive) const
	{
		return SearchCase == ESearchCase::CaseSensitive ? FCString::Strcmp(**this, *Other)
														: FCString::Stricmp(**this, *Other);
	}

	// Searching ------------------------------------------------------------------------------------------------------

	/** Index of SubStr, or INDEX_NONE. StartPosition: where to begin (FromEnd searches before it). */
	int32 Find(const TCHAR* SubStr, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase,
		ESearchDir::Type SearchDir = ESearchDir::FromStart, int32 StartPosition = INDEX_NONE) const;
	FORCEINLINE int32 Find(const FString& SubStr, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase,
		ESearchDir::Type SearchDir = ESearchDir::FromStart, int32 StartPosition = INDEX_NONE) const
	{
		return Find(*SubStr, SearchCase, SearchDir, StartPosition);
	}

	FORCEINLINE bool Contains(const TCHAR* SubStr, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase,
		ESearchDir::Type SearchDir = ESearchDir::FromStart) const
	{
		return Find(SubStr, SearchCase, SearchDir) != INDEX_NONE;
	}
	FORCEINLINE bool Contains(const FString& SubStr, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase,
		ESearchDir::Type SearchDir = ESearchDir::FromStart) const
	{
		return Find(*SubStr, SearchCase, SearchDir) != INDEX_NONE;
	}

	/** First occurrence of a character; false when absent. */
	FORCEINLINE bool FindChar(TCHAR InChar, int32& Index) const
	{
		return Data.Find(InChar, Index);
	}

	/** Last occurrence of a character; false when absent. */
	FORCEINLINE bool FindLastChar(TCHAR InChar, int32& Index) const
	{
		return Data.FindLast(InChar, Index);
	}

	template <typename Predicate>
	FORCEINLINE int32 FindLastCharByPredicate(Predicate Pred, int32 Count) const
	{
		check(Count >= 0 && Count <= this->Len());
		return Data.FindLastByPredicate(Pred, Count);
	}

	template <typename Predicate>
	FORCEINLINE int32 FindLastCharByPredicate(Predicate Pred) const
	{
		return Data.FindLastByPredicate(Pred, this->Len());
	}

	bool StartsWith(const TCHAR* InPrefix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	bool StartsWith(const FString& InPrefix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	bool EndsWith(const TCHAR* InSuffix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;
	bool EndsWith(const FString& InSuffix, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const;

	/** Splits at the first (or last) InS; false when InS is absent. */
	bool Split(const FString& InS, FString* LeftS, FString* RightS,
		ESearchCase::Type SearchCase = ESearchCase::IgnoreCase,
		ESearchDir::Type SearchDir = ESearchDir::FromStart) const;

	// Substrings -----------------------------------------------------------------------------------------------------

	FORCEINLINE FString Left(int32 Count) const
	{
		return FString(FMath::Clamp(Count, 0, Len()), **this);
	}
	FORCEINLINE void LeftInline(int32 Count, bool bAllowShrinking = true)
	{
		const int32 Length = Len();
		Count = FMath::Clamp(Count, 0, Length);
		RemoveAt(Count, Length - Count, bAllowShrinking);
	}

	FORCEINLINE FString LeftChop(int32 Count) const
	{
		const int32 Length = Len();
		return FString(FMath::Clamp(Length - Count, 0, Length), **this);
	}
	FORCEINLINE void LeftChopInline(int32 Count, bool bAllowShrinking = true)
	{
		const int32 Length = Len();
		RemoveAt(FMath::Clamp(Length - Count, 0, Length), Count, bAllowShrinking);
	}

	FORCEINLINE FString Right(int32 Count) const
	{
		const int32 Length = Len();
		return FString(**this + Length - FMath::Clamp(Count, 0, Length));
	}
	FORCEINLINE void RightInline(int32 Count, bool bAllowShrinking = true)
	{
		const int32 Length = Len();
		RemoveAt(0, Length - FMath::Clamp(Count, 0, Length), bAllowShrinking);
	}

	FORCEINLINE FString RightChop(int32 Count) const
	{
		const int32 Length = Len();
		return FString(**this + Length - FMath::Clamp(Length - Count, 0, Length));
	}
	FORCEINLINE void RightChopInline(int32 Count, bool bAllowShrinking = true)
	{
		RemoveAt(0, Count, bAllowShrinking);
	}

	/** Count characters from Start (clamped to the string). */
	FString Mid(int32 Start, int32 Count = 0x7fffffff) const;
	void MidInline(int32 Start, int32 Count = 0x7fffffff, bool bAllowShrinking = true);

	// Transformations ------------------------------------------------------------------------------------------------

	FString ToUpper() const&;
	FString ToUpper() &&;
	void ToUpperInline();
	FString ToLower() const&;
	FString ToLower() &&;
	void ToLowerInline();

	/** Pads with spaces on the left / right to ChCount characters. */
	FString LeftPad(int32 ChCount) const;
	FString RightPad(int32 ChCount) const;

	void TrimStartAndEndInline();
	FString TrimStartAndEnd() const&;
	FString TrimStartAndEnd() &&;
	void TrimStartInline();
	FString TrimStart() const&;
	FString TrimStart() &&;
	void TrimEndInline();
	FString TrimEnd() const&;
	FString TrimEnd() &&;

	/** Removes surrounding double quotes. */
	void TrimQuotesInline(bool* bQuotesRemoved = nullptr);
	FString TrimQuotes(bool* bQuotesRemoved = nullptr) const;

	/** Replaces every From with To (UE: Replace). */
	FString Replace(const TCHAR* From, const TCHAR* To, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) const&;
	FString Replace(const TCHAR* From, const TCHAR* To, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase) &&;

	/** In-place Replace; returns the number of replacements. */
	int32 ReplaceInline(
		const TCHAR* SearchText, const TCHAR* ReplacementText, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);

	/** Replaces a character everywhere. */
	void ReplaceCharInline(
		TCHAR SearchChar, TCHAR ReplacementChar, ESearchCase::Type SearchCase = ESearchCase::IgnoreCase);

	FString Reverse() const&;
	FString Reverse() &&;
	void ReverseString();

	/** Splits on a delimiter string; returns the number of parts (UE: ParseIntoArray). */
	int32 ParseIntoArray(TArray<FString>& OutArray, const TCHAR* pchDelim, bool InCullEmpty = true) const;

	/** Splits on any of several delimiters. */
	int32 ParseIntoArray(
		TArray<FString>& OutArray, const TCHAR* const* DelimArray, int32 NumDelims, bool InCullEmpty = true) const;

	/** Splits on whitespace (and optional extra delimiters). */
	int32 ParseIntoArrayWS(
		TArray<FString>& OutArray, const TCHAR* pchExtraDelim = nullptr, bool InCullEmpty = true) const;

	/** Splits on line endings (\r\n, \r, \n). */
	int32 ParseIntoArrayLines(TArray<FString>& OutArray, bool InCullEmpty = true) const;

	/** Optional sign, digits, optional '.' and digits. */
	FORCEINLINE bool IsNumeric() const
	{
		return !IsEmpty() && FCString::IsNumeric(Data.GetData());
	}

	/** "true" / "yes" / "on" / non-zero number. */
	FORCEINLINE bool ToBool() const
	{
		return FCString::ToBool(**this);
	}

	// Construction helpers -------------------------------------------------------------------------------------------

	/** printf-style formatted string (UE: FString::Printf). */
	static FString Printf(const TCHAR* Fmt, ...) LEON_PRINTF_FORMAT(1, 2);

	/** printf-style with a va_list. */
	static FString PrintfImpl(const TCHAR* Fmt, va_list Args);

	/** Joins the elements with a separator; elements are FStrings or TCHAR strings (UE: Join). */
	template <typename RangeType>
	static FString Join(const RangeType& Range, const TCHAR* Separator)
	{
		FString Result;
		bool bFirst = true;
		for (const auto& Element : Range)
		{
			if (bFirst)
			{
				bFirst = false;
			}
			else
			{
				Result += Separator;
			}
			Result += Element;
		}
		return Result;
	}

	static FString FromInt(int32 Num);

	/** A float with at least InMinFractionalDigits decimals and no trailing zeros beyond them (UE). */
	static FString SanitizeFloat(double InFloat, const int32 InMinFractionalDigits = 1);

	static FORCEINLINE FString Chr(TCHAR Ch)
	{
		const TCHAR Temp[2] = {Ch, 0};
		return FString(Temp);
	}

	static FString ChrN(int32 NumCharacters, TCHAR Char);

	/** Integer with thousands separators ("1,234,567"). */
	static FString FormatAsNumber(int32 InNumber);

	FORCEINLINE friend uint32 GetTypeHash(const FString& S)
	{
		return FCrc::Strihash_DEPRECATED(*S);
	}

	// Concatenation --------------------------------------------------------------------------------------------------

	FORCEINLINE friend FString operator+(const FString& Lhs, const FString& Rhs)
	{
		return ConcatFStrings(Lhs, *Rhs, Rhs.Len());
	}
	FORCEINLINE friend FString operator+(FString&& Lhs, const FString& Rhs)
	{
		Lhs += Rhs;
		return MoveTemp(Lhs);
	}
	FORCEINLINE friend FString operator+(const FString& Lhs, const TCHAR* Rhs)
	{
		return ConcatFStrings(Lhs, Rhs, Rhs ? FCString::Strlen(Rhs) : 0);
	}
	FORCEINLINE friend FString operator+(FString&& Lhs, const TCHAR* Rhs)
	{
		Lhs += Rhs;
		return MoveTemp(Lhs);
	}
	FORCEINLINE friend FString operator+(const TCHAR* Lhs, const FString& Rhs)
	{
		FString Result(Lhs, Rhs.Len());
		Result += Rhs;
		return Result;
	}

	/** Path concatenation with exactly one '/' between (UE: operator/). */
	FORCEINLINE friend FString operator/(const FString& Lhs, const TCHAR* Rhs)
	{
		const int32 StrLength = FCString::Strlen(Rhs);
		FString Result(Lhs, StrLength + 1);
		Result.PathAppend(Rhs, StrLength);
		return Result;
	}
	FORCEINLINE friend FString operator/(FString&& Lhs, const TCHAR* Rhs)
	{
		Lhs.PathAppend(Rhs, FCString::Strlen(Rhs));
		return MoveTemp(Lhs);
	}
	FORCEINLINE friend FString operator/(const FString& Lhs, const FString& Rhs)
	{
		FString Result(Lhs, Rhs.Len() + 1);
		Result.PathAppend(Rhs.Data.GetData(), Rhs.Len());
		return Result;
	}
	FORCEINLINE friend FString operator/(FString&& Lhs, const FString& Rhs)
	{
		Lhs.PathAppend(Rhs.Data.GetData(), Rhs.Len());
		return MoveTemp(Lhs);
	}
	FORCEINLINE friend FString operator/(const TCHAR* Lhs, const FString& Rhs)
	{
		FString Result(Lhs, Rhs.Len() + 1);
		Result.PathAppend(Rhs.Data.GetData(), Rhs.Len());
		return Result;
	}

private:
	/** FString(Str) with ExtraSlack characters reserved. */
	FString(const TCHAR* Str, int32 ExtraSlack);

	static FString ConcatFStrings(const FString& Lhs, const TCHAR* Rhs, int32 RhsLen);
};

template <>
struct TIsZeroConstructType<FString>
{
	enum
	{
		Value = true
	};
};

template <>
struct TIsContiguousContainer<FString>
{
	enum
	{
		Value = true
	};
};

// Lexical conversions (UE: LexToString / LexFromString / LexTryParseString).

inline const FString& LexToString(const FString& Str)
{
	return Str;
}
inline FString LexToString(FString&& Str)
{
	return MoveTemp(Str);
}
inline FString LexToString(const TCHAR* Str)
{
	return FString(Str);
}
inline FString LexToString(bool bValue)
{
	return bValue ? FString(TEXT("true")) : FString(TEXT("false"));
}

template <typename T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, int> = 0>
inline FString LexToString(const T& Value)
{
	if constexpr (std::is_floating_point_v<T>)
	{
		return FString::Printf("%f", double(Value));
	}
	else if constexpr (std::is_signed_v<T>)
	{
		return FString::Printf("%lld", (long long)Value);
	}
	else
	{
		return FString::Printf("%llu", (unsigned long long)Value);
	}
}

inline FString LexToSanitizedString(float Value)
{
	return FString::SanitizeFloat(Value);
}
inline FString LexToSanitizedString(double Value)
{
	return FString::SanitizeFloat(Value);
}

inline void LexFromString(int8& OutValue, const TCHAR* Buffer)
{
	OutValue = int8(FCString::Atoi(Buffer));
}
inline void LexFromString(int16& OutValue, const TCHAR* Buffer)
{
	OutValue = int16(FCString::Atoi(Buffer));
}
inline void LexFromString(int32& OutValue, const TCHAR* Buffer)
{
	OutValue = FCString::Atoi(Buffer);
}
inline void LexFromString(int64& OutValue, const TCHAR* Buffer)
{
	OutValue = FCString::Atoi64(Buffer);
}
inline void LexFromString(uint8& OutValue, const TCHAR* Buffer)
{
	OutValue = uint8(FCString::Atoi(Buffer));
}
inline void LexFromString(uint16& OutValue, const TCHAR* Buffer)
{
	OutValue = uint16(FCString::Atoi(Buffer));
}
inline void LexFromString(uint32& OutValue, const TCHAR* Buffer)
{
	OutValue = uint32(FCString::Atoi64(Buffer));
}
inline void LexFromString(uint64& OutValue, const TCHAR* Buffer)
{
	OutValue = FCString::Strtoui64(Buffer, nullptr, 0);
}
inline void LexFromString(float& OutValue, const TCHAR* Buffer)
{
	OutValue = FCString::Atof(Buffer);
}
inline void LexFromString(double& OutValue, const TCHAR* Buffer)
{
	OutValue = FCString::Atod(Buffer);
}
inline void LexFromString(bool& OutValue, const TCHAR* Buffer)
{
	OutValue = FCString::ToBool(Buffer);
}
inline void LexFromString(FString& OutValue, const TCHAR* Buffer)
{
	OutValue = Buffer;
}

/** Parses a number; false (OutValue untouched) when Buffer is not entirely numeric (UE: LexTryParseString). */
template <typename T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, int> = 0>
bool LexTryParseString(T& OutValue, const TCHAR* Buffer)
{
	if (FCString::IsNumeric(Buffer))
	{
		LexFromString(OutValue, Buffer);
		return true;
	}
	return false;
}

inline bool LexTryParseString(bool& OutValue, const TCHAR* Buffer)
{
	if (FCString::Stricmp(Buffer, TEXT("true")) == 0 || FCString::Stricmp(Buffer, TEXT("false")) == 0 ||
		FCString::IsNumeric(Buffer))
	{
		OutValue = FCString::ToBool(Buffer);
		return true;
	}
	return false;
}

/** '0'-'9' / 'A'-'F' for a value 0-15 (UE: NibbleToTChar). */
inline TCHAR NibbleToTChar(uint8 Num)
{
	if (Num > 9)
	{
		return TCHAR('A' + (Num - 10));
	}
	return TCHAR('0' + Num);
}

/** Appends two upper-case hex digits (UE: ByteToHex). */
inline void ByteToHex(uint8 In, FString& Result)
{
	Result += NibbleToTChar(uint8(In >> 4));
	Result += NibbleToTChar(uint8(In & 15));
}

/** Upper-case hex of Count bytes (UE: BytesToHex). */
inline FString BytesToHex(const uint8* In, int32 Count)
{
	FString Result;
	Result.Reserve(Count * 2);
	while (Count)
	{
		ByteToHex(*In++, Result);
		Count--;
	}
	return Result;
}

/** Whether Char is a hex digit (UE: CheckTCharIsHex). */
inline bool CheckTCharIsHex(const TCHAR Char)
{
	return (Char >= '0' && Char <= '9') || (Char >= 'A' && Char <= 'F') || (Char >= 'a' && Char <= 'f');
}

/** Value of a hex digit, 0 for anything else (UE: TCharToNibble). */
inline uint8 TCharToNibble(const TCHAR Char)
{
	if (Char >= '0' && Char <= '9')
	{
		return uint8(Char - '0');
	}
	if (Char >= 'A' && Char <= 'F')
	{
		return uint8(Char - 'A' + 10);
	}
	if (Char >= 'a' && Char <= 'f')
	{
		return uint8(Char - 'a' + 10);
	}
	return 0;
}

/** Bytes of a hex string; an odd length pads the first nibble (UE: HexToBytes). Returns the byte count. */
inline int32 HexToBytes(const FString& HexString, uint8* OutBytes)
{
	int32 NumBytes = 0;
	const bool bPadNibble = (HexString.Len() % 2) == 1;
	const TCHAR* CharPos = *HexString;
	if (bPadNibble)
	{
		OutBytes[NumBytes++] = TCharToNibble(*CharPos++);
	}
	while (*CharPos)
	{
		OutBytes[NumBytes] = uint8(TCharToNibble(*CharPos++) << 4);
		OutBytes[NumBytes] = uint8(OutBytes[NumBytes] + TCharToNibble(*CharPos++));
		++NumBytes;
	}
	return NumBytes;
}
