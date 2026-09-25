#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "JsonGlobals.h"
#include "Misc/CString.h"
#include "Misc/Char.h"
#include "Templates/SharedPointer.h"

/**
 * Reads JSON text one token at a time (UE: TJsonReader). ReadNext returns false at the end of the root value or on
 * an error (GetErrorMessage names the line and character). Inside an object, GetIdentifier is the field name of the
 * token just read. Leon reads TCHAR (UTF-8) text only; "\u" escapes become UTF-8.
 */
template <class CharType = TCHAR>
class TJsonReader
{
public:
	virtual ~TJsonReader() = default;

	bool ReadNext(EJsonNotation& Notation)
	{
		if (!ErrorMessage.IsEmpty())
		{
			Notation = EJsonNotation::Error;
			return false;
		}

		Identifier.Reset();
		SkipWhitespace();

		if (bFinishedRoot)
		{
			if (Pos < Content.Len())
			{
				SetError("Unexpected character after the root value");
				Notation = EJsonNotation::Error;
			}
			return false;
		}

		if (ParseState.Num() > 0)
		{
			const bool bInObject = ParseState.Last() == EJson::Object;
			const TCHAR Close = bInObject ? '}' : ']';
			TCHAR C = Peek();

			if (C == Close)
			{
				if (bExpectValueAfterComma)
				{
					return Fail(Notation, "Trailing comma");
				}
				++Pos;
				ParseState.Pop();
				FirstInContainer.Pop();
				Notation = bInObject ? EJsonNotation::ObjectEnd : EJsonNotation::ArrayEnd;
				AfterValue();
				return true;
			}

			if (!FirstInContainer.Last())
			{
				if (C != ',')
				{
					return Fail(Notation, bInObject ? "Expected ',' or '}'" : "Expected ',' or ']'");
				}
				++Pos;
				SkipWhitespace();
				bExpectValueAfterComma = true;
				C = Peek();
			}

			if (bInObject)
			{
				if (C != '"')
				{
					return Fail(Notation, "Expected a field name");
				}
				if (!ParseString(Identifier))
				{
					Notation = EJsonNotation::Error;
					return false;
				}
				SkipWhitespace();
				if (Peek() != ':')
				{
					return Fail(Notation, "Expected ':'");
				}
				++Pos;
				SkipWhitespace();
			}
		}

		return ReadValue(Notation);
	}

	/** Skips the rest of the object just started (UE: SkipObject). */
	bool SkipObject()
	{
		return SkipContainer(EJsonNotation::ObjectEnd);
	}

	/** Skips the rest of the array just started (UE: SkipArray). */
	bool SkipArray()
	{
		return SkipContainer(EJsonNotation::ArrayEnd);
	}

	const FString& GetIdentifier() const
	{
		return Identifier;
	}

	const FString& GetValueAsString() const
	{
		return StringValue;
	}

	double GetValueAsNumber() const
	{
		return NumberValue;
	}

	/** The number as written in the text. */
	const FString& GetValueAsNumberString() const
	{
		return StringValue;
	}

	bool GetValueAsBoolean() const
	{
		return BoolValue;
	}

	const FString& GetErrorMessage() const
	{
		return ErrorMessage;
	}

	uint32 GetLineNumber() const
	{
		return LineNumber;
	}

	uint32 GetCharacterNumber() const
	{
		return CharacterNumber;
	}

protected:
	explicit TJsonReader(const FString& InContent)
		: Content(InContent)
	{
	}

private:
	TCHAR Peek() const
	{
		return Pos < Content.Len() ? Content[Pos] : TCHAR(0);
	}

	void SkipWhitespace()
	{
		while (Pos < Content.Len())
		{
			const TCHAR C = Content[Pos];
			if (C != ' ' && C != '\t' && C != '\r' && C != '\n')
			{
				break;
			}
			++Pos;
		}
	}

	void SetError(const TCHAR* Message)
	{
		// Line and character of the error position, both from 1.
		LineNumber = 1;
		CharacterNumber = 1;
		for (int32 Index = 0; Index < Pos && Index < Content.Len(); ++Index)
		{
			if (Content[Index] == '\n')
			{
				++LineNumber;
				CharacterNumber = 1;
			}
			else
			{
				++CharacterNumber;
			}
		}
		ErrorMessage = FString::Printf("%s. Line: %u Ch: %u", Message, LineNumber, CharacterNumber);
	}

	bool Fail(EJsonNotation& Notation, const TCHAR* Message)
	{
		SetError(Message);
		Notation = EJsonNotation::Error;
		return false;
	}

	void AfterValue()
	{
		bExpectValueAfterComma = false;
		if (ParseState.Num() == 0)
		{
			bFinishedRoot = true;
		}
		else
		{
			FirstInContainer.Last() = false;
		}
	}

	bool ReadValue(EJsonNotation& Notation)
	{
		const TCHAR C = Peek();
		switch (C)
		{
			case '{':
			case '[':
				if (ParseState.Num() >= MaxDepth)
				{
					return Fail(Notation, "Too deeply nested");
				}
				++Pos;
				ParseState.Push(C == '{' ? EJson::Object : EJson::Array);
				FirstInContainer.Push(true);
				bExpectValueAfterComma = false;
				Notation = C == '{' ? EJsonNotation::ObjectStart : EJsonNotation::ArrayStart;
				return true;

			case '"':
				if (!ParseString(StringValue))
				{
					Notation = EJsonNotation::Error;
					return false;
				}
				Notation = EJsonNotation::String;
				AfterValue();
				return true;

			case 't':
			case 'f':
			case 'n':
			{
				const TCHAR* Word = C == 't' ? "true" : (C == 'f' ? "false" : "null");
				const int32 WordLen = FCString::Strlen(Word);
				if (Content.Len() - Pos < WordLen || FCString::Strncmp(&Content[Pos], Word, SIZE_T(WordLen)) != 0)
				{
					return Fail(Notation, "Unexpected character");
				}
				Pos += WordLen;
				BoolValue = C == 't';
				Notation = C == 'n' ? EJsonNotation::Null : EJsonNotation::Boolean;
				AfterValue();
				return true;
			}

			default:
				if (C == '-' || FChar::IsDigit(C))
				{
					if (!ParseNumber())
					{
						Notation = EJsonNotation::Error;
						return false;
					}
					Notation = EJsonNotation::Number;
					AfterValue();
					return true;
				}
				return Fail(Notation, C == 0 ? "Unexpected end of input" : "Unexpected character");
		}
	}

	static int32 HexValue(TCHAR C)
	{
		if (C >= '0' && C <= '9')
		{
			return C - '0';
		}
		if (C >= 'a' && C <= 'f')
		{
			return C - 'a' + 10;
		}
		if (C >= 'A' && C <= 'F')
		{
			return C - 'A' + 10;
		}
		return -1;
	}

	bool ReadHex4(uint32& OutValue)
	{
		OutValue = 0;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const int32 Digit = HexValue(Peek());
			if (Digit < 0)
			{
				SetError("Bad \\u escape");
				return false;
			}
			OutValue = OutValue * 16 + uint32(Digit);
			++Pos;
		}
		return true;
	}

	static void AppendUtf8(FString& Out, uint32 CodePoint)
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

	bool ParseString(FString& Out)
	{
		Out.Reset();
		++Pos; // Opening quote.
		for (;;)
		{
			if (Pos >= Content.Len())
			{
				SetError("Unterminated string");
				return false;
			}
			const TCHAR C = Content[Pos++];
			if (C == '"')
			{
				return true;
			}
			if (C != '\\')
			{
				Out.AppendChar(C);
				continue;
			}

			const TCHAR Escape = Peek();
			++Pos;
			switch (Escape)
			{
				case '"':
					Out.AppendChar('"');
					break;
				case '\\':
					Out.AppendChar('\\');
					break;
				case '/':
					Out.AppendChar('/');
					break;
				case 'b':
					Out.AppendChar('\b');
					break;
				case 'f':
					Out.AppendChar('\f');
					break;
				case 'n':
					Out.AppendChar('\n');
					break;
				case 'r':
					Out.AppendChar('\r');
					break;
				case 't':
					Out.AppendChar('\t');
					break;
				case 'u':
				{
					uint32 CodePoint = 0;
					if (!ReadHex4(CodePoint))
					{
						return false;
					}
					// A UTF-16 surrogate pair spells one code point.
					if (CodePoint >= 0xD800 && CodePoint <= 0xDBFF && Peek() == '\\' && Pos + 1 < Content.Len() &&
						Content[Pos + 1] == 'u')
					{
						const int32 Saved = Pos;
						Pos += 2;
						uint32 Low = 0;
						if (ReadHex4(Low) && Low >= 0xDC00 && Low <= 0xDFFF)
						{
							CodePoint = 0x10000 + ((CodePoint - 0xD800) << 10) + (Low - 0xDC00);
						}
						else
						{
							ErrorMessage.Reset();
							Pos = Saved;
						}
					}
					AppendUtf8(Out, CodePoint);
					break;
				}
				default:
					--Pos;
					SetError("Bad escape sequence");
					return false;
			}
		}
	}

	bool ParseNumber()
	{
		const int32 Start = Pos;
		if (Peek() == '-')
		{
			++Pos;
		}
		if (!FChar::IsDigit(Peek()))
		{
			SetError("Bad number");
			return false;
		}
		while (FChar::IsDigit(Peek()))
		{
			++Pos;
		}
		if (Peek() == '.')
		{
			++Pos;
			if (!FChar::IsDigit(Peek()))
			{
				SetError("Bad number");
				return false;
			}
			while (FChar::IsDigit(Peek()))
			{
				++Pos;
			}
		}
		if (Peek() == 'e' || Peek() == 'E')
		{
			++Pos;
			if (Peek() == '+' || Peek() == '-')
			{
				++Pos;
			}
			if (!FChar::IsDigit(Peek()))
			{
				SetError("Bad number");
				return false;
			}
			while (FChar::IsDigit(Peek()))
			{
				++Pos;
			}
		}
		StringValue = Content.Mid(Start, Pos - Start);
		NumberValue = FCString::Atod(*StringValue);
		return true;
	}

	bool SkipContainer(EJsonNotation EndNotation)
	{
		const int32 Depth = ParseState.Num();
		EJsonNotation Notation;
		while (ReadNext(Notation))
		{
			if (ParseState.Num() < Depth && Notation == EndNotation)
			{
				return true;
			}
		}
		return false;
	}

	static constexpr int32 MaxDepth = 256;

	FString Content;
	int32 Pos = 0;

	TArray<EJson> ParseState;
	TArray<bool> FirstInContainer;
	bool bExpectValueAfterComma = false;
	bool bFinishedRoot = false;

	FString Identifier;
	FString StringValue;
	double NumberValue = 0.0;
	bool BoolValue = false;

	FString ErrorMessage;
	uint32 LineNumber = 1;
	uint32 CharacterNumber = 1;
};

/** A reader over a string (UE: TJsonStringReader). */
template <class CharType = TCHAR>
class TJsonStringReader : public TJsonReader<CharType>
{
public:
	static TSharedRef<TJsonStringReader<CharType>> Create(const FString& JsonString)
	{
		return MakeShareable(new TJsonStringReader<CharType>(JsonString));
	}

protected:
	explicit TJsonStringReader(const FString& JsonString)
		: TJsonReader<CharType>(JsonString)
	{
	}
};

/** Makes readers (UE: TJsonReaderFactory). */
template <class CharType = TCHAR>
class TJsonReaderFactory
{
public:
	static TSharedRef<TJsonReader<CharType>> Create(const FString& JsonString)
	{
		return TJsonStringReader<CharType>::Create(JsonString);
	}
};
