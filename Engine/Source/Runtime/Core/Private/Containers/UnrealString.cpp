#include "Containers/UnrealString.h"

#include "Containers/StringConv.h"
#include "Math/UnrealMathUtility.h"

#include <cmath>
#include <cstdio>

FString::FString(const TCHAR* Str)
{
	if (Str && *Str)
	{
		const int32 Length = FCString::Strlen(Str);
		Data.AddUninitialized(Length + 1);
		FMemory::Memcpy(Data.GetData(), Str, (Length + 1) * sizeof(TCHAR));
	}
}

FString::FString(const TCHAR* Str, int32 ExtraSlack)
{
	const int32 Length = (Str && *Str) ? FCString::Strlen(Str) : 0;
	if (Length || ExtraSlack)
	{
		Data.Reserve(Length + ExtraSlack + 1);
		Data.AddUninitialized(Length + 1);
		if (Length)
		{
			FMemory::Memcpy(Data.GetData(), Str, Length * sizeof(TCHAR));
		}
		Data[Length] = 0;
		if (!Length)
		{
			// Keep FString("") empty: no terminator-only storage.
			Data.SetNumUnsafeInternal(0);
		}
	}
}

FString::FString(int32 InCount, const TCHAR* InSrc)
{
	check(InCount >= 0);
	if (InCount > 0 && InSrc && *InSrc)
	{
		// Stop at an embedded terminator like UE does.
		const int32 Length = FCString::Strnlen(InSrc, SIZE_T(InCount));
		Data.AddUninitialized(Length + 1);
		FMemory::Memcpy(Data.GetData(), InSrc, Length * sizeof(TCHAR));
		Data[Length] = 0;
	}
}

FString::FString(const WIDECHAR* Str)
{
	if (Str && *Str)
	{
		FWideToTCHAR Converted(Str);
		AppendChars(Converted.Get(), Converted.Length());
	}
}

FString& FString::operator=(const TCHAR* Other)
{
	if (Data.GetData() != Other)
	{
		const int32 Length = (Other && *Other) ? FCString::Strlen(Other) : 0;
		Data.Empty(Length ? Length + 1 : 0);
		if (Length)
		{
			Data.AddUninitialized(Length + 1);
			FMemory::Memcpy(Data.GetData(), Other, (Length + 1) * sizeof(TCHAR));
		}
	}
	return *this;
}

FString& FString::AppendChar(TCHAR InChar)
{
	CheckInvariants();
	if (InChar != 0)
	{
		// Position to insert the character. At the end of the string but before the null terminator.
		const int32 InsertIndex = (Data.Num() > 0) ? Data.Num() - 1 : 0;
		// Number of characters to add. If we don't have any existing characters, we'll need to append the
		// terminating zero as well.
		const int32 InsertCount = (Data.Num() > 0) ? 1 : 2;
		Data.AddUninitialized(InsertCount);
		Data[InsertIndex] = InChar;
		Data[InsertIndex + 1] = 0;
	}
	return *this;
}

void FString::AppendChars(const TCHAR* Array, int32 Count)
{
	check(Count >= 0);
	if (Count <= 0)
	{
		return;
	}
	checkSlow(Array);

	const int32 Index = Data.Num();

	// Reserve enough space - including an extra gap for a null terminator if we don't already have a string allocated.
	Data.AddUninitialized(Count + (Index ? 0 : 1));

	TCHAR* EndPtr = Data.GetData() + Index - (Index ? 1 : 0);

	// Copy characters to end of string, overwriting null terminator if we already have one.
	FMemory::Memcpy(EndPtr, Array, Count * sizeof(TCHAR));

	// (Re-)establish the null terminator.
	*(EndPtr + Count) = 0;
}

void FString::PathAppend(const TCHAR* Str, int32 StrLength)
{
	int32 DataNum = Data.Num();
	if (StrLength == 0)
	{
		if (DataNum > 1 && Data[DataNum - 2] != TEXT('/') && Data[DataNum - 2] != TEXT('\\'))
		{
			Data[DataNum - 1] = TEXT('/');
			Data.Add(TEXT('\0'));
		}
	}
	else
	{
		if (DataNum > 0)
		{
			if (DataNum > 1 && Data[DataNum - 2] != TEXT('/') && Data[DataNum - 2] != TEXT('\\') && *Str != TEXT('/'))
			{
				Data[DataNum - 1] = TEXT('/');
			}
			else
			{
				Data.Pop(false);
				--DataNum;
			}
		}

		Reserve(DataNum + StrLength);
		Data.Append(Str, StrLength);
		Data.Add(TEXT('\0'));
	}
}

FString& FString::Appendf(const TCHAR* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	Append(PrintfImpl(Fmt, Args));
	va_end(Args);
	return *this;
}

void FString::AppendInt(int32 InNum)
{
	TCHAR Buffer[16];
	FCString::Snprintf(Buffer, 16, "%d", InNum);
	Append(Buffer);
}

void FString::InsertAt(int32 Index, const FString& Characters)
{
	if (Characters.Len())
	{
		if (Data.Num() == 0)
		{
			*this += Characters;
		}
		else
		{
			Data.Insert(Characters.Data.GetData(), Characters.Len(), Index);
		}
	}
}

bool FString::RemoveFromStart(const TCHAR* InPrefix, ESearchCase::Type SearchCase)
{
	if (!InPrefix || *InPrefix == 0)
	{
		return false;
	}
	if (StartsWith(InPrefix, SearchCase))
	{
		RemoveAt(0, FCString::Strlen(InPrefix));
		return true;
	}
	return false;
}

bool FString::RemoveFromStart(const FString& InPrefix, ESearchCase::Type SearchCase)
{
	if (InPrefix.IsEmpty())
	{
		return false;
	}
	if (StartsWith(InPrefix, SearchCase))
	{
		RemoveAt(0, InPrefix.Len());
		return true;
	}
	return false;
}

bool FString::RemoveFromEnd(const TCHAR* InSuffix, ESearchCase::Type SearchCase)
{
	if (!InSuffix || *InSuffix == 0)
	{
		return false;
	}
	if (EndsWith(InSuffix, SearchCase))
	{
		const int32 SuffixLen = FCString::Strlen(InSuffix);
		RemoveAt(Len() - SuffixLen, SuffixLen);
		return true;
	}
	return false;
}

bool FString::RemoveFromEnd(const FString& InSuffix, ESearchCase::Type SearchCase)
{
	if (InSuffix.IsEmpty())
	{
		return false;
	}
	if (EndsWith(InSuffix, SearchCase))
	{
		RemoveAt(Len() - InSuffix.Len(), InSuffix.Len());
		return true;
	}
	return false;
}

int32 FString::Find(
	const TCHAR* SubStr, ESearchCase::Type SearchCase, ESearchDir::Type SearchDir, int32 StartPosition) const
{
	if (SubStr == nullptr)
	{
		return INDEX_NONE;
	}
	if (SearchDir == ESearchDir::FromStart)
	{
		const TCHAR* Start = **this;
		if (StartPosition != INDEX_NONE)
		{
			Start += FMath::Clamp(StartPosition, 0, Len() - 1);
		}
		const TCHAR* Tmp =
			SearchCase == ESearchCase::IgnoreCase ? FCString::Stristr(Start, SubStr) : FCString::Strstr(Start, SubStr);
		return Tmp ? int32(Tmp - **this) : INDEX_NONE;
	}

	// Search backwards: the last occurrence that ends at or before StartPosition.
	if (IsEmpty())
	{
		return INDEX_NONE;
	}
	const int32 SearchStringLength = FMath::Max(1, FCString::Strlen(SubStr));
	if (StartPosition == INDEX_NONE || StartPosition >= Len())
	{
		StartPosition = Len();
	}
	for (int32 Index = StartPosition - SearchStringLength; Index >= 0; Index--)
	{
		const bool bMatch = SearchCase == ESearchCase::IgnoreCase
			? FCString::Strnicmp(**this + Index, SubStr, SearchStringLength) == 0
			: FCString::Strncmp(**this + Index, SubStr, SearchStringLength) == 0;
		if (bMatch)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool FString::StartsWith(const TCHAR* InPrefix, ESearchCase::Type SearchCase) const
{
	if (!InPrefix)
	{
		return false;
	}
	const int32 PrefixLen = FCString::Strlen(InPrefix);
	if (PrefixLen > Len())
	{
		return false;
	}
	return SearchCase == ESearchCase::IgnoreCase ? FCString::Strnicmp(**this, InPrefix, PrefixLen) == 0
												 : FCString::Strncmp(**this, InPrefix, PrefixLen) == 0;
}

bool FString::StartsWith(const FString& InPrefix, ESearchCase::Type SearchCase) const
{
	if (InPrefix.Len() > Len())
	{
		return false;
	}
	return SearchCase == ESearchCase::IgnoreCase ? FCString::Strnicmp(**this, *InPrefix, InPrefix.Len()) == 0
												 : FCString::Strncmp(**this, *InPrefix, InPrefix.Len()) == 0;
}

bool FString::EndsWith(const TCHAR* InSuffix, ESearchCase::Type SearchCase) const
{
	if (!InSuffix)
	{
		return false;
	}
	const int32 SuffixLen = FCString::Strlen(InSuffix);
	if (SuffixLen > Len())
	{
		return false;
	}
	const TCHAR* StrPtr = **this + Len() - SuffixLen;
	return SearchCase == ESearchCase::IgnoreCase ? FCString::Stricmp(StrPtr, InSuffix) == 0
												 : FCString::Strcmp(StrPtr, InSuffix) == 0;
}

bool FString::EndsWith(const FString& InSuffix, ESearchCase::Type SearchCase) const
{
	return EndsWith(*InSuffix, SearchCase);
}

bool FString::Split(
	const FString& InS, FString* LeftS, FString* RightS, ESearchCase::Type SearchCase, ESearchDir::Type SearchDir) const
{
	const int32 InPos = Find(InS, SearchCase, SearchDir);
	if (InPos < 0)
	{
		return false;
	}
	if (LeftS)
	{
		if (LeftS != this)
		{
			*LeftS = Left(InPos);
			if (RightS)
			{
				*RightS = Mid(InPos + InS.Len());
			}
		}
		else
		{
			// We know that RightS can't be this so we can safely modify it before we deal with LeftS.
			if (RightS)
			{
				*RightS = Mid(InPos + InS.Len());
			}
			*LeftS = Left(InPos);
		}
	}
	else if (RightS)
	{
		*RightS = Mid(InPos + InS.Len());
	}
	return true;
}

FString FString::Mid(int32 Start, int32 Count) const
{
	if (Count >= 0)
	{
		const int32 Length = Len();
		const int32 RequestedStart = Start;
		Start = FMath::Clamp(Start, 0, Length);
		const int32 End = int32(FMath::Clamp(int64(Count) + RequestedStart, int64(Start), int64(Length)));
		return FString(End - Start, **this + Start);
	}
	return FString();
}

void FString::MidInline(int32 Start, int32 Count, bool bAllowShrinking)
{
	if (Count != 0x7fffffff && int64(Start) + Count < 0x7fffffff)
	{
		LeftInline(Count + Start, false);
	}
	RightChopInline(Start, bAllowShrinking);
}

FString FString::ToUpper() const&
{
	FString New(*this);
	New.ToUpperInline();
	return New;
}

FString FString::ToUpper() &&
{
	ToUpperInline();
	return MoveTemp(*this);
}

void FString::ToUpperInline()
{
	const int32 StringLength = Len();
	TCHAR* RawData = Data.GetData();
	for (int32 Index = 0; Index < StringLength; ++Index)
	{
		RawData[Index] = FChar::ToUpper(RawData[Index]);
	}
}

FString FString::ToLower() const&
{
	FString New(*this);
	New.ToLowerInline();
	return New;
}

FString FString::ToLower() &&
{
	ToLowerInline();
	return MoveTemp(*this);
}

void FString::ToLowerInline()
{
	const int32 StringLength = Len();
	TCHAR* RawData = Data.GetData();
	for (int32 Index = 0; Index < StringLength; ++Index)
	{
		RawData[Index] = FChar::ToLower(RawData[Index]);
	}
}

FString FString::LeftPad(int32 ChCount) const
{
	const int32 Pad = ChCount - Len();
	if (Pad > 0)
	{
		return ChrN(Pad, TEXT(' ')) + *this;
	}
	return *this;
}

FString FString::RightPad(int32 ChCount) const
{
	const int32 Pad = ChCount - Len();
	if (Pad > 0)
	{
		return *this + ChrN(Pad, TEXT(' '));
	}
	return *this;
}

void FString::TrimStartAndEndInline()
{
	TrimEndInline();
	TrimStartInline();
}

FString FString::TrimStartAndEnd() const&
{
	FString Result(*this);
	Result.TrimStartAndEndInline();
	return Result;
}

FString FString::TrimStartAndEnd() &&
{
	TrimStartAndEndInline();
	return MoveTemp(*this);
}

void FString::TrimStartInline()
{
	int32 Pos = 0;
	while (Pos < Len() && FChar::IsWhitespace((*this)[Pos]))
	{
		Pos++;
	}
	RemoveAt(0, Pos);
}

FString FString::TrimStart() const&
{
	FString Result(*this);
	Result.TrimStartInline();
	return Result;
}

FString FString::TrimStart() &&
{
	TrimStartInline();
	return MoveTemp(*this);
}

void FString::TrimEndInline()
{
	int32 End = Len();
	while (End > 0 && FChar::IsWhitespace((*this)[End - 1]))
	{
		End--;
	}
	RemoveAt(End, Len() - End);
}

FString FString::TrimEnd() const&
{
	FString Result(*this);
	Result.TrimEndInline();
	return Result;
}

FString FString::TrimEnd() &&
{
	TrimEndInline();
	return MoveTemp(*this);
}

void FString::TrimQuotesInline(bool* bQuotesRemoved)
{
	int32 Start = 0;
	int32 Count = Len();
	bool bQuotesWereRemoved = false;
	if (Count > 0)
	{
		if ((*this)[0] == TCHAR('"'))
		{
			Start++;
			Count--;
			bQuotesWereRemoved = true;
		}
		if (Len() > 1 && (*this)[Len() - 1] == TCHAR('"'))
		{
			Count--;
			bQuotesWereRemoved = true;
		}
	}
	if (bQuotesRemoved != nullptr)
	{
		*bQuotesRemoved = bQuotesWereRemoved;
	}
	MidInline(Start, Count, false);
}

FString FString::TrimQuotes(bool* bQuotesRemoved) const
{
	FString Result(*this);
	Result.TrimQuotesInline(bQuotesRemoved);
	return Result;
}

FString FString::Replace(const TCHAR* From, const TCHAR* To, ESearchCase::Type SearchCase) const&
{
	FString Copy(*this);
	Copy.ReplaceInline(From, To, SearchCase);
	return Copy;
}

FString FString::Replace(const TCHAR* From, const TCHAR* To, ESearchCase::Type SearchCase) &&
{
	ReplaceInline(From, To, SearchCase);
	return MoveTemp(*this);
}

int32 FString::ReplaceInline(const TCHAR* SearchText, const TCHAR* ReplacementText, ESearchCase::Type SearchCase)
{
	int32 ReplacementCount = 0;
	if (Len() > 0 && SearchText != nullptr && *SearchText != 0 && ReplacementText != nullptr &&
		(SearchCase == ESearchCase::IgnoreCase || FCString::Strcmp(SearchText, ReplacementText) != 0))
	{
		const int32 SearchLen = FCString::Strlen(SearchText);
		const int32 ReplaceLen = FCString::Strlen(ReplacementText);

		FString Result;
		Result.Reserve(Len());
		int32 Position = 0;
		for (;;)
		{
			const int32 Found = Find(SearchText, SearchCase, ESearchDir::FromStart, Position);
			if (Found == INDEX_NONE)
			{
				break;
			}
			Result.AppendChars(**this + Position, Found - Position);
			Result.AppendChars(ReplacementText, ReplaceLen);
			Position = Found + SearchLen;
			++ReplacementCount;
			if (Position >= Len())
			{
				break;
			}
		}
		if (ReplacementCount)
		{
			Result.AppendChars(**this + Position, Len() - Position);
			*this = MoveTemp(Result);
		}
	}
	return ReplacementCount;
}

void FString::ReplaceCharInline(TCHAR SearchChar, TCHAR ReplacementChar, ESearchCase::Type SearchCase)
{
	const int32 StringLength = Len();
	TCHAR* RawData = Data.GetData();
	if (SearchCase == ESearchCase::IgnoreCase && FChar::IsAlpha(SearchChar))
	{
		const TCHAR Lower = FChar::ToLower(SearchChar);
		for (int32 Index = 0; Index < StringLength; ++Index)
		{
			if (FChar::ToLower(RawData[Index]) == Lower)
			{
				RawData[Index] = ReplacementChar;
			}
		}
	}
	else
	{
		for (int32 Index = 0; Index < StringLength; ++Index)
		{
			if (RawData[Index] == SearchChar)
			{
				RawData[Index] = ReplacementChar;
			}
		}
	}
}

FString FString::Reverse() const&
{
	FString New(*this);
	New.ReverseString();
	return New;
}

FString FString::Reverse() &&
{
	ReverseString();
	return MoveTemp(*this);
}

void FString::ReverseString()
{
	if (Len() > 0)
	{
		TCHAR* StartChar = &(*this)[0];
		TCHAR* EndChar = &(*this)[Len() - 1];
		while (StartChar < EndChar)
		{
			const TCHAR TempChar = *StartChar;
			*StartChar = *EndChar;
			*EndChar = TempChar;
			StartChar++;
			EndChar--;
		}
	}
}

int32 FString::ParseIntoArray(TArray<FString>& OutArray, const TCHAR* pchDelim, bool InCullEmpty) const
{
	// Make sure the delimit string is not null or empty.
	check(pchDelim);
	OutArray.Reset();
	const TCHAR* Start = **this;
	const int32 DelimLength = FCString::Strlen(pchDelim);
	if (Start && *Start != TEXT('\0') && DelimLength)
	{
		while (const TCHAR* At = FCString::Strstr(Start, pchDelim))
		{
			if (!InCullEmpty || At - Start)
			{
				OutArray.Emplace(int32(At - Start), Start);
			}
			Start = At + DelimLength;
		}
		if (!InCullEmpty || *Start)
		{
			OutArray.Emplace(Start);
		}
	}
	return OutArray.Num();
}

int32 FString::ParseIntoArray(
	TArray<FString>& OutArray, const TCHAR* const* DelimArray, int32 NumDelims, bool InCullEmpty) const
{
	// Make sure the delimit string is not null or empty.
	check(DelimArray);
	OutArray.Reset();
	const TCHAR* Start = **this;
	const int32 Length = Len();
	if (Start)
	{
		int32 SubstringBeginIndex = 0;

		// Iterate through string.
		for (int32 Index = 0; Index < Length;)
		{
			int32 SubstringEndIndex = INDEX_NONE;
			int32 DelimiterLength = 0;

			// Attempt each delimiter.
			for (int32 DelimIndex = 0; DelimIndex < NumDelims; ++DelimIndex)
			{
				DelimiterLength = FCString::Strlen(DelimArray[DelimIndex]);

				// If we found a delimiter...
				if (DelimiterLength && FCString::Strncmp(Start + Index, DelimArray[DelimIndex], DelimiterLength) == 0)
				{
					// Mark the end of the substring.
					SubstringEndIndex = Index;
					break;
				}
			}

			if (SubstringEndIndex != INDEX_NONE)
			{
				const int32 SubstringLength = SubstringEndIndex - SubstringBeginIndex;
				// If we're not culling empty strings or if we are but the string isn't empty anyways...
				if (!InCullEmpty || SubstringLength != 0)
				{
					// ... add new string from substring beginning up to the beginning of this delimiter.
					OutArray.Emplace(SubstringLength, Start + SubstringBeginIndex);
				}
				// Next substring begins at the end of the discovered delimiter.
				SubstringBeginIndex = SubstringEndIndex + DelimiterLength;
				Index = SubstringBeginIndex;
			}
			else
			{
				++Index;
			}
		}

		// Add any remaining characters after the last delimiter.
		const int32 SubstringLength = Length - SubstringBeginIndex;
		// If we're not culling empty strings or if we are but the string isn't empty anyways...
		if (!InCullEmpty || SubstringLength != 0)
		{
			// ... add new string from substring beginning up to the beginning of this delimiter.
			OutArray.Emplace(Start + SubstringBeginIndex);
		}
	}
	return OutArray.Num();
}

int32 FString::ParseIntoArrayWS(TArray<FString>& OutArray, const TCHAR* pchExtraDelim, bool InCullEmpty) const
{
	// Default array of White Spaces, the last entry can be replaced with the optional pchExtraDelim string
	// (if you want to split on white space and another character).
	const TCHAR* WhiteSpace[] = {TEXT(" "), TEXT("\t"), TEXT("\r"), TEXT("\n"), TEXT("")};

	// Start with just the standard whitespaces.
	int32 NumWhiteSpaces = UE_ARRAY_COUNT(WhiteSpace) - 1;
	// Add one more if there was an extra delimiter passed in.
	if (pchExtraDelim && *pchExtraDelim)
	{
		WhiteSpace[NumWhiteSpaces++] = pchExtraDelim;
	}

	return ParseIntoArray(OutArray, WhiteSpace, NumWhiteSpaces, InCullEmpty);
}

int32 FString::ParseIntoArrayLines(TArray<FString>& OutArray, bool InCullEmpty) const
{
	// Default array of LineEndings.
	static const TCHAR* LineEndings[] = {
		TEXT("\r\n"),
		TEXT("\r"),
		TEXT("\n"),
	};

	// Start with just the standard line endings.
	const int32 NumLineEndings = UE_ARRAY_COUNT(LineEndings);
	return ParseIntoArray(OutArray, LineEndings, NumLineEndings, InCullEmpty);
}

FString FString::PrintfImpl(const TCHAR* Fmt, va_list Args)
{
	TCHAR StackBuffer[512];
	va_list ArgsCopy;
	va_copy(ArgsCopy, Args);
	const int Needed = std::vsnprintf(StackBuffer, sizeof(StackBuffer), Fmt, ArgsCopy);
	va_end(ArgsCopy);
	if (Needed < 0)
	{
		return FString();
	}
	if (Needed < int(sizeof(StackBuffer)))
	{
		return FString(Needed, StackBuffer);
	}

	FString Result;
	Result.Data.AddUninitialized(Needed + 1);
	std::vsnprintf(Result.Data.GetData(), SIZE_T(Needed) + 1, Fmt, Args);
	return Result;
}

FString FString::Printf(const TCHAR* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	FString Result = PrintfImpl(Fmt, Args);
	va_end(Args);
	return Result;
}

FString FString::FromInt(int32 Num)
{
	FString Ret;
	Ret.AppendInt(Num);
	return Ret;
}

FString FString::SanitizeFloat(double InFloat, const int32 InMinFractionalDigits)
{
	// Avoids negative zero.
	if (InFloat == 0.0)
	{
		InFloat = 0.0;
	}

	// First create the string.
	FString TempString = Printf("%f", InFloat);
	if (!TempString.IsNumeric())
	{
		// String is something like "inf", "nan" or "-nan"; return it as is.
		return TempString;
	}

	// Trim all trailing zeros (up-to and including the decimal separator) from the fractional part of the number.
	int32 TrimIndex = INDEX_NONE;
	int32 DecimalSeparatorIndex = INDEX_NONE;
	for (int32 CharIndex = TempString.Len() - 1; CharIndex >= 0; --CharIndex)
	{
		const TCHAR Char = TempString[CharIndex];
		if (Char == TEXT('.'))
		{
			DecimalSeparatorIndex = CharIndex;
			TrimIndex = FMath::Max(TrimIndex, DecimalSeparatorIndex);
			break;
		}
		if (TrimIndex == INDEX_NONE && Char != TEXT('0'))
		{
			TrimIndex = CharIndex + 1;
		}
	}
	check(TrimIndex != INDEX_NONE && DecimalSeparatorIndex != INDEX_NONE);
	TempString.RemoveAt(TrimIndex, TempString.Len() - TrimIndex, /*bAllowShrinking*/ false);

	// Pad the number back to the minimum number of fractional digits.
	if (InMinFractionalDigits > 0)
	{
		if (TrimIndex == DecimalSeparatorIndex)
		{
			// Re-add the decimal separator.
			TempString.AppendChar(TEXT('.'));
		}

		const int32 NumFractionalDigits = (TempString.Len() - DecimalSeparatorIndex) - 1;
		const int32 FractionalDigitsToPad = InMinFractionalDigits - NumFractionalDigits;
		if (FractionalDigitsToPad > 0)
		{
			TempString.Reserve(TempString.Len() + FractionalDigitsToPad);
			for (int32 Cx = 0; Cx < FractionalDigitsToPad; ++Cx)
			{
				TempString.AppendChar(TEXT('0'));
			}
		}
	}

	return TempString;
}

FString FString::ChrN(int32 NumCharacters, TCHAR Char)
{
	check(NumCharacters >= 0);
	FString Temp;
	Temp.Data.AddUninitialized(NumCharacters + 1);
	for (int32 Cx = 0; Cx < NumCharacters; ++Cx)
	{
		Temp.Data[Cx] = Char;
	}
	Temp.Data[NumCharacters] = TEXT('\0');
	if (NumCharacters == 0)
	{
		Temp.Data.Empty();
	}
	return Temp;
}

FString FString::FormatAsNumber(int32 InNumber)
{
	FString Number = FromInt(InNumber);
	FString Result;

	int32 Dec = 0;
	for (int32 Index = Number.Len() - 1; Index >= 0; --Index)
	{
		if (Dec == 3 && Number[Index] != TEXT('-'))
		{
			Result.InsertAt(0, TEXT(','));
			Dec = 0;
		}
		Result.InsertAt(0, Number[Index]);
		++Dec;
	}
	return Result;
}

FString FString::ConcatFStrings(const FString& Lhs, const TCHAR* Rhs, int32 RhsLen)
{
	FString Result(Lhs, RhsLen);
	Result.AppendChars(Rhs, RhsLen);
	return Result;
}
