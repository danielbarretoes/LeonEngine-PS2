#include "Misc/Parse.h"

#include "Misc/CString.h"
#include "Misc/Char.h"
#include "Misc/Guid.h"

namespace
{
	/** Appends a code point as UTF-8 (TCHAR is UTF-8). */
	void AppendCodePoint(FString& Out, uint32 CodePoint)
	{
		if (CodePoint < 0x80)
		{
			Out.AppendChar(TCHAR(CodePoint));
		}
		else if (CodePoint < 0x800)
		{
			Out.AppendChar(TCHAR(0xC0 | (CodePoint >> 6)));
			Out.AppendChar(TCHAR(0x80 | (CodePoint & 0x3F)));
		}
		else if (CodePoint < 0x10000)
		{
			Out.AppendChar(TCHAR(0xE0 | (CodePoint >> 12)));
			Out.AppendChar(TCHAR(0x80 | ((CodePoint >> 6) & 0x3F)));
			Out.AppendChar(TCHAR(0x80 | (CodePoint & 0x3F)));
		}
		else
		{
			Out.AppendChar(TCHAR(0xF0 | (CodePoint >> 18)));
			Out.AppendChar(TCHAR(0x80 | ((CodePoint >> 12) & 0x3F)));
			Out.AppendChar(TCHAR(0x80 | ((CodePoint >> 6) & 0x3F)));
			Out.AppendChar(TCHAR(0x80 | (CodePoint & 0x3F)));
		}
	}

	/** Start of the text after Match, or nullptr (numeric values do not skip quoted text, like UE). */
	const TCHAR* FindValueStart(const TCHAR* Stream, const TCHAR* Match)
	{
		const TCHAR* Found = FCString::Strifind(Stream, Match);
		return Found != nullptr ? Found + FCString::Strlen(Match) : nullptr;
	}
} // namespace

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, FString& Value, bool bShouldStopOnSeparator)
{
	if (Stream == nullptr || Match == nullptr)
	{
		return false;
	}

	const TCHAR* Found = FCString::Strifind(Stream, Match, true);
	if (Found == nullptr)
	{
		return false;
	}
	const TCHAR* Start = Found + FCString::Strlen(Match);

	// Check for quoted arguments' string with spaces: -Option="Value1 Value2".
	if (*Start == '"')
	{
		// Skip quote character if only params were quoted; """Value""" keeps inner quotes out as well.
		const bool bTripleQuoted = FCString::Strncmp(Start, "\"\"\"", 3) == 0;
		Start += bTripleQuoted ? 3 : 1;
		const TCHAR* End = FCString::Strstr(Start, bTripleQuoted ? "\"\"\"" : "\"");
		Value = End != nullptr ? FString(int32(End - Start), Start) : FString(Start);
		return true;
	}

	// Non-quoted string without spaces.
	const TCHAR* End = Start;
	while (*End && *End != ' ' && *End != '\r' && *End != '\n' && *End != '\t' &&
		!(bShouldStopOnSeparator && (*End == ',' || *End == ')')))
	{
		++End;
	}
	Value = FString(int32(End - Start), Start);
	return true;
}

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, TCHAR* Value, int32 MaxLen, bool bShouldStopOnSeparator)
{
	if (MaxLen <= 0)
	{
		return false;
	}
	Value[0] = 0;

	FString Result;
	if (!FParse::Value(Stream, Match, Result, bShouldStopOnSeparator))
	{
		return false;
	}
	FCString::Strncpy(Value, *Result, SIZE_T(MaxLen));
	return true;
}

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, FName& Name)
{
	FString Result;
	if (!FParse::Value(Stream, Match, Result))
	{
		return false;
	}
	Name = FName(*Result);
	return true;
}

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, uint8& Value)
{
	const TCHAR* Temp = FindValueStart(Stream, Match);
	if (Temp == nullptr)
	{
		return false;
	}
	Value = uint8(FCString::Atoi(Temp));
	return Value != 0 || FChar::IsDigit(Temp[0]);
}

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, int8& Value)
{
	const TCHAR* Temp = FindValueStart(Stream, Match);
	if (Temp == nullptr)
	{
		return false;
	}
	Value = int8(FCString::Atoi(Temp));
	return Value != 0 || FChar::IsDigit(Temp[0]);
}

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, uint16& Value)
{
	const TCHAR* Temp = FindValueStart(Stream, Match);
	if (Temp == nullptr)
	{
		return false;
	}
	Value = uint16(FCString::Atoi(Temp));
	return Value != 0 || FChar::IsDigit(Temp[0]);
}

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, int16& Value)
{
	const TCHAR* Temp = FindValueStart(Stream, Match);
	if (Temp == nullptr)
	{
		return false;
	}
	Value = int16(FCString::Atoi(Temp));
	return Value != 0 || FChar::IsDigit(Temp[0]);
}

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, uint32& Value)
{
	const TCHAR* Temp = FindValueStart(Stream, Match);
	if (Temp == nullptr)
	{
		return false;
	}
	Value = uint32(FCString::Strtoui64(Temp, nullptr, 10));
	return Value != 0 || FChar::IsDigit(Temp[0]);
}

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, int32& Value)
{
	const TCHAR* Temp = FindValueStart(Stream, Match);
	if (Temp == nullptr)
	{
		return false;
	}
	Value = FCString::Atoi(Temp);
	return Value != 0 || FChar::IsDigit(Temp[0]);
}

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, uint64& Value)
{
	const TCHAR* Temp = FindValueStart(Stream, Match);
	if (Temp == nullptr)
	{
		return false;
	}
	Value = FCString::Strtoui64(Temp, nullptr, 10);
	return Value != 0 || FChar::IsDigit(Temp[0]);
}

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, int64& Value)
{
	const TCHAR* Temp = FindValueStart(Stream, Match);
	if (Temp == nullptr)
	{
		return false;
	}
	Value = FCString::Atoi64(Temp);
	return Value != 0 || FChar::IsDigit(Temp[0]);
}

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, float& Value)
{
	const TCHAR* Temp = FindValueStart(Stream, Match);
	if (Temp == nullptr)
	{
		return false;
	}
	Value = FCString::Atof(Temp);
	return true;
}

bool FParse::Value(const TCHAR* Stream, const TCHAR* Match, FGuid& Guid)
{
	FString Temp;
	if (!FParse::Value(Stream, Match, Temp))
	{
		return false;
	}
	return FGuid::Parse(Temp, Guid);
}

bool FParse::Bool(const TCHAR* Stream, const TCHAR* Match, bool& OnOff)
{
	FString Temp;
	if (!FParse::Value(Stream, Match, Temp))
	{
		return false;
	}
	OnOff = FCString::ToBool(*Temp);
	return true;
}

bool FParse::Param(const TCHAR* Stream, const TCHAR* Param)
{
	const TCHAR* Start = Stream;
	if (*Stream == 0)
	{
		return false;
	}
	while ((Start = FCString::Strifind(Start + 1, Param, true)) != nullptr)
	{
		// The switch character must start a word: at the beginning or after whitespace.
		if (Start > Stream && (Start[-1] == '-' || Start[-1] == '/') &&
			(Stream > (Start - 2) || FChar::IsWhitespace(Start[-2])))
		{
			const TCHAR* End = Start + FCString::Strlen(Param);
			if (*End == 0 || FChar::IsWhitespace(*End))
			{
				return true;
			}
		}
	}
	return false;
}

bool FParse::Command(const TCHAR** Stream, const TCHAR* Match, bool /*bParseMightTriggerExecution*/)
{
	while (**Stream == ' ' || **Stream == '\t')
	{
		(*Stream)++;
	}

	const int32 MatchLen = FCString::Strlen(Match);
	if (FCString::Strnicmp(*Stream, Match, SIZE_T(MatchLen)) == 0)
	{
		*Stream += MatchLen;
		if (!FChar::IsAlnum(**Stream))
		{
			while (**Stream == ' ' || **Stream == '\t')
			{
				(*Stream)++;
			}
			return true;
		}
		// Only found partial match.
		*Stream -= MatchLen;
	}
	return false;
}

bool FParse::Token(const TCHAR*& Str, FString& Arg, bool bUseEscape)
{
	Arg.Reset();

	// Skip preceding spaces and tabs.
	while (FChar::IsWhitespace(*Str))
	{
		Str++;
	}

	if (*Str == '"')
	{
		// Get quoted string.
		Str++;
		while (*Str && *Str != '"')
		{
			TCHAR C = *Str++;
			if (C == '\\' && bUseEscape)
			{
				// Get escape.
				C = *Str++;
				if (!C)
				{
					break;
				}
			}
			Arg.AppendChar(C);
		}
		if (*Str == '"')
		{
			Str++;
		}
		return Arg.Len() > 0;
	}

	// Get unquoted string (that might contain a quoted part, which will be left intact). For example,
	// -ARG="foo bar baz" is one token, with the quotes intact.
	bool bInQuote = false;
	while (*Str != 0 && (!FChar::IsWhitespace(*Str) || bInQuote))
	{
		if (*Str == '"')
		{
			bInQuote = !bInQuote;
		}
		Arg.AppendChar(*Str++);
	}
	return Arg.Len() > 0;
}

bool FParse::Token(const TCHAR*& Str, TCHAR* Result, int32 MaxLen, bool bUseEscape)
{
	FString Arg;
	const bool bFound = Token(Str, Arg, bUseEscape);
	if (MaxLen > 0)
	{
		FCString::Strncpy(Result, *Arg, SIZE_T(MaxLen));
	}
	return bFound && Arg.Len() > 0;
}

FString FParse::Token(const TCHAR*& Str, bool bUseEscape)
{
	FString Arg;
	Token(Str, Arg, bUseEscape);
	return Arg;
}

bool FParse::AlnumToken(const TCHAR*& Str, FString& Arg)
{
	Arg.Reset();

	// Skip preceding spaces and tabs.
	while (FChar::IsWhitespace(*Str))
	{
		Str++;
	}
	while (FChar::IsAlnum(*Str) || *Str == '_')
	{
		Arg.AppendChar(*Str++);
	}
	return Arg.Len() > 0;
}

bool FParse::Line(const TCHAR** Stream, FString& Result, bool bExact)
{
	bool bGotStream = false;
	bool bIsQuoted = false;
	bool bIgnore = false;

	Result.Reset();

	while (**Stream != 0 && **Stream != '\n' && **Stream != '\r')
	{
		// Start of comments.
		if (!bIsQuoted && !bExact && (*Stream)[0] == '/' && (*Stream)[1] == '/')
		{
			bIgnore = true;
		}

		// Command chaining.
		if (!bIsQuoted && !bExact && **Stream == '|')
		{
			break;
		}

		// Check quoting.
		bIsQuoted = bIsQuoted ^ (**Stream == '"');
		bGotStream = true;

		// Got stuff.
		if (!bIgnore)
		{
			Result.AppendChar(*((*Stream)++));
		}
		else
		{
			(*Stream)++;
		}
	}

	if (**Stream == '\r')
	{
		(*Stream)++;
	}
	if (**Stream == '\n')
	{
		(*Stream)++;
	}

	return **Stream != 0 || bGotStream;
}

bool FParse::QuotedString(const TCHAR* Buffer, FString& Value, int32* OutNumCharsRead)
{
	if (OutNumCharsRead)
	{
		*OutNumCharsRead = 0;
	}

	const TCHAR* Start = Buffer;

	// Require opening quote.
	if (*Buffer++ != '"')
	{
		return false;
	}

	auto ShouldParse = [](const TCHAR Ch) { return Ch != 0 && Ch != '"' && Ch != '\n' && Ch != '\r'; };

	while (ShouldParse(*Buffer))
	{
		if (*Buffer != '\\')
		{
			// Unescaped character.
			Value.AppendChar(*Buffer++);
		}
		else if (*++Buffer == '\\')
		{
			Value.AppendChar('\\');
			++Buffer;
		}
		else if (*Buffer == '"')
		{
			Value.AppendChar('"');
			++Buffer;
		}
		else if (*Buffer == '\'')
		{
			Value.AppendChar('\'');
			++Buffer;
		}
		else if (*Buffer == 'n')
		{
			Value.AppendChar('\n');
			++Buffer;
		}
		else if (*Buffer == 'r')
		{
			Value.AppendChar('\r');
			++Buffer;
		}
		else if (*Buffer == 't')
		{
			Value.AppendChar('\t');
			++Buffer;
		}
		else if (FChar::IsOctDigit(*Buffer))
		{
			// Octal sequence (\012).
			uint32 Code = 0;
			for (int32 Digits = 0; Digits < 3 && ShouldParse(*Buffer) && FChar::IsOctDigit(*Buffer); ++Digits)
			{
				Code = Code * 8 + uint32(*Buffer++ - '0');
			}
			AppendCodePoint(Value, Code);
		}
		else if ((*Buffer == 'x' || *Buffer == 'u') && FChar::IsHexDigit(*(Buffer + 1)))
		{
			// Hex (\xBEEF) or UTF-16 (ሴ) sequence, written as UTF-8.
			const int32 MaxDigits = *Buffer == 'u' ? 4 : 8;
			++Buffer;
			uint32 Code = 0;
			for (int32 Digits = 0; Digits < MaxDigits && ShouldParse(*Buffer) && FChar::IsHexDigit(*Buffer); ++Digits)
			{
				Code = Code * 16 + uint32(HexDigit(*Buffer++));
			}
			AppendCodePoint(Value, Code);
		}
		else
		{
			// Unhandled escape sequence.
			Value.AppendChar('\\');
			Value.AppendChar(*Buffer++);
		}
	}

	// Require closing quote.
	if (*Buffer++ != '"')
	{
		return false;
	}

	if (OutNumCharsRead)
	{
		*OutNumCharsRead = int32(Buffer - Start);
	}
	return true;
}

int32 FParse::HexDigit(TCHAR C)
{
	if (C >= '0' && C <= '9')
	{
		return C - '0';
	}
	if (C >= 'a' && C <= 'f')
	{
		return C + 10 - 'a';
	}
	if (C >= 'A' && C <= 'F')
	{
		return C + 10 - 'A';
	}
	return 0;
}

uint32 FParse::HexNumber(const TCHAR* HexString)
{
	uint32 Ret = 0;
	while (*HexString)
	{
		Ret *= 16;
		Ret += uint32(HexDigit(*HexString++));
	}
	return Ret;
}

uint64 FParse::HexNumber64(const TCHAR* HexString)
{
	uint64 Ret = 0;
	while (*HexString)
	{
		Ret *= 16;
		Ret += uint64(HexDigit(*HexString++));
	}
	return Ret;
}

void FParse::Next(const TCHAR** Stream)
{
	// Skip over spaces, tabs, cr's, and linefeeds.
	for (;;)
	{
		while (**Stream == ' ' || **Stream == '\t' || **Stream == '\r' || **Stream == '\n')
		{
			++*Stream;
		}

		if (**Stream == ';' || ((*Stream)[0] == '/' && (*Stream)[1] == '/'))
		{
			// Skip past comments.
			while (**Stream != 0 && **Stream != '\n' && **Stream != '\r')
			{
				++*Stream;
			}
			continue;
		}
		break;
	}
}
