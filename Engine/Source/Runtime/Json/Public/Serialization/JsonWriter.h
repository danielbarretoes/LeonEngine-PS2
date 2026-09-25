#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "JsonGlobals.h"
#include "Misc/AssertionMacros.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Templates/SharedPointer.h"

/** The last thing a writer wrote (UE: EJsonToken). */
enum class EJsonToken
{
	None,
	Comma,
	CurlyOpen,
	CurlyClose,
	SquareOpen,
	SquareClose,
	Colon,
	String,
	Number,
	True,
	False,
	Null,
	Identifier
};

inline bool EJsonToken_IsShortValue(EJsonToken Token)
{
	return Token == EJsonToken::Number || Token == EJsonToken::True || Token == EJsonToken::False ||
		Token == EJsonToken::Null;
}

/**
 * Writes JSON into a string (UE: TJsonWriter). The layout follows UE: fields on their own lines, short values (numbers,
 * booleans, null) of an array on one line, strings and objects of an array on their own lines.
 */
template <class CharType = TCHAR, class PrintPolicy = TPrettyJsonPrintPolicy<CharType>>
class TJsonWriter
{
public:
	static TSharedRef<TJsonWriter> Create(FString* const Stream, int32 InitialIndentLevel = 0)
	{
		return MakeShareable(new TJsonWriter(Stream, InitialIndentLevel));
	}

	virtual ~TJsonWriter() = default;

	int32 GetIndentLevel() const
	{
		return IndentLevel;
	}

	bool CanWriteValueWithoutIdentifier() const
	{
		return Stack.Num() <= 0 || Stack.Top() == EJson::Array || PreviousTokenWritten == EJsonToken::Identifier;
	}

	void WriteObjectStart()
	{
		check(CanWriteValueWithoutIdentifier());
		if (PreviousTokenWritten != EJsonToken::None)
		{
			WriteCommaIfNeeded();
		}
		if (PreviousTokenWritten != EJsonToken::None && PreviousTokenWritten != EJsonToken::Identifier)
		{
			PrintPolicy::WriteLineTerminator(Stream);
			PrintPolicy::WriteTabs(Stream, IndentLevel);
		}
		PrintPolicy::WriteChar(Stream, '{');
		++IndentLevel;
		Stack.Push(EJson::Object);
		PreviousTokenWritten = EJsonToken::CurlyOpen;
	}

	void WriteObjectStart(const FString& Identifier)
	{
		check(Stack.Num() > 0 && Stack.Top() == EJson::Object);
		WriteIdentifier(Identifier);
		PrintPolicy::WriteLineTerminator(Stream);
		PrintPolicy::WriteTabs(Stream, IndentLevel);
		PrintPolicy::WriteChar(Stream, '{');
		++IndentLevel;
		Stack.Push(EJson::Object);
		PreviousTokenWritten = EJsonToken::CurlyOpen;
	}

	void WriteObjectEnd()
	{
		check(Stack.Num() > 0 && Stack.Top() == EJson::Object);
		PrintPolicy::WriteLineTerminator(Stream);
		--IndentLevel;
		PrintPolicy::WriteTabs(Stream, IndentLevel);
		PrintPolicy::WriteChar(Stream, '}');
		Stack.Pop();
		PreviousTokenWritten = EJsonToken::CurlyClose;
	}

	void WriteArrayStart()
	{
		check(CanWriteValueWithoutIdentifier());
		if (PreviousTokenWritten != EJsonToken::None)
		{
			WriteCommaIfNeeded();
		}
		if (PreviousTokenWritten != EJsonToken::None && PreviousTokenWritten != EJsonToken::Identifier)
		{
			PrintPolicy::WriteLineTerminator(Stream);
			PrintPolicy::WriteTabs(Stream, IndentLevel);
		}
		PrintPolicy::WriteChar(Stream, '[');
		++IndentLevel;
		Stack.Push(EJson::Array);
		PreviousTokenWritten = EJsonToken::SquareOpen;
	}

	void WriteArrayStart(const FString& Identifier)
	{
		check(Stack.Num() > 0 && Stack.Top() == EJson::Object);
		WriteIdentifier(Identifier);
		PrintPolicy::WriteSpace(Stream);
		PrintPolicy::WriteChar(Stream, '[');
		++IndentLevel;
		Stack.Push(EJson::Array);
		PreviousTokenWritten = EJsonToken::SquareOpen;
	}

	void WriteArrayEnd()
	{
		check(Stack.Num() > 0 && Stack.Top() == EJson::Array);
		--IndentLevel;
		if (PreviousTokenWritten == EJsonToken::SquareOpen || EJsonToken_IsShortValue(PreviousTokenWritten))
		{
			PrintPolicy::WriteSpace(Stream);
		}
		else
		{
			PrintPolicy::WriteLineTerminator(Stream);
			PrintPolicy::WriteTabs(Stream, IndentLevel);
		}
		PrintPolicy::WriteChar(Stream, ']');
		Stack.Pop();
		PreviousTokenWritten = EJsonToken::SquareClose;
	}

	// Values in an array (or the root).
	void WriteValue(bool Value)
	{
		WriteValueStart(true);
		PreviousTokenWritten = WriteValueOnly(Value);
	}
	void WriteValue(int32 Value)
	{
		WriteValue(int64(Value));
	}
	void WriteValue(uint32 Value)
	{
		WriteValue(int64(Value));
	}
	void WriteValue(int64 Value)
	{
		WriteValueStart(true);
		PreviousTokenWritten = WriteValueOnly(Value);
	}
	void WriteValue(float Value)
	{
		WriteValue(double(Value));
	}
	void WriteValue(double Value)
	{
		WriteValueStart(true);
		PreviousTokenWritten = WriteValueOnly(Value);
	}
	void WriteValue(const FString& Value)
	{
		WriteValueStart(false);
		PreviousTokenWritten = WriteValueOnly(Value);
	}
	void WriteValue(const TCHAR* Value)
	{
		WriteValue(FString(Value));
	}
	void WriteNull()
	{
		WriteValueStart(true);
		PrintPolicy::WriteString(Stream, FString("null"));
		PreviousTokenWritten = EJsonToken::Null;
	}

	// Fields of an object.
	template <class ValueType>
	void WriteValue(const FString& Identifier, ValueType Value)
	{
		check(Stack.Num() > 0 && Stack.Top() == EJson::Object);
		WriteIdentifier(Identifier);
		PrintPolicy::WriteSpace(Stream);
		PreviousTokenWritten = WriteValueOnly(Value);
	}

	void WriteValue(const FString& Identifier, const TArray<FString>& Array)
	{
		WriteArrayStart(Identifier);
		for (const FString& Element : Array)
		{
			WriteValue(Element);
		}
		WriteArrayEnd();
	}

	void WriteNull(const FString& Identifier)
	{
		check(Stack.Num() > 0 && Stack.Top() == EJson::Object);
		WriteIdentifier(Identifier);
		PrintPolicy::WriteSpace(Stream);
		PrintPolicy::WriteString(Stream, FString("null"));
		PreviousTokenWritten = EJsonToken::Null;
	}

	/** Writes already formatted JSON as the field's value (UE: WriteRawJSONValue). */
	void WriteRawJSONValue(const FString& Identifier, const FString& Value)
	{
		check(Stack.Num() > 0 && Stack.Top() == EJson::Object);
		WriteIdentifier(Identifier);
		PrintPolicy::WriteSpace(Stream);
		PrintPolicy::WriteString(Stream, Value);
		PreviousTokenWritten = EJsonToken::String;
	}

	/** True when every object and array was closed (UE: Close). */
	bool Close()
	{
		return (PreviousTokenWritten == EJsonToken::None || PreviousTokenWritten == EJsonToken::CurlyClose ||
				   PreviousTokenWritten == EJsonToken::SquareClose) &&
			Stack.Num() == 0;
	}

protected:
	TJsonWriter(FString* const InStream, int32 InitialIndentLevel)
		: Stream(InStream)
		, IndentLevel(InitialIndentLevel)
	{
	}

	void WriteCommaIfNeeded()
	{
		if (PreviousTokenWritten != EJsonToken::CurlyOpen && PreviousTokenWritten != EJsonToken::SquareOpen &&
			PreviousTokenWritten != EJsonToken::Identifier)
		{
			PrintPolicy::WriteChar(Stream, ',');
		}
	}

	void WriteValueStart(bool bIsShortValue)
	{
		check(CanWriteValueWithoutIdentifier());
		if (PreviousTokenWritten == EJsonToken::Identifier || PreviousTokenWritten == EJsonToken::None)
		{
			return;
		}
		WriteCommaIfNeeded();
		if (bIsShortValue &&
			(PreviousTokenWritten == EJsonToken::SquareOpen || EJsonToken_IsShortValue(PreviousTokenWritten)))
		{
			PrintPolicy::WriteSpace(Stream);
		}
		else
		{
			PrintPolicy::WriteLineTerminator(Stream);
			PrintPolicy::WriteTabs(Stream, IndentLevel);
		}
	}

	void WriteIdentifier(const FString& Identifier)
	{
		WriteCommaIfNeeded();
		PrintPolicy::WriteLineTerminator(Stream);
		PrintPolicy::WriteTabs(Stream, IndentLevel);
		WriteStringValue(Identifier);
		PrintPolicy::WriteChar(Stream, ':');
		PreviousTokenWritten = EJsonToken::Identifier;
	}

	EJsonToken WriteValueOnly(bool Value)
	{
		PrintPolicy::WriteString(Stream, Value ? FString("true") : FString("false"));
		return Value ? EJsonToken::True : EJsonToken::False;
	}

	EJsonToken WriteValueOnly(int32 Value)
	{
		return WriteValueOnly(int64(Value));
	}

	EJsonToken WriteValueOnly(uint32 Value)
	{
		return WriteValueOnly(int64(Value));
	}

	EJsonToken WriteValueOnly(int64 Value)
	{
		PrintPolicy::WriteString(Stream, FString::Printf("%lld", (long long)Value));
		return EJsonToken::Number;
	}

	EJsonToken WriteValueOnly(float Value)
	{
		PrintPolicy::WriteFloat(Stream, Value);
		return EJsonToken::Number;
	}

	EJsonToken WriteValueOnly(double Value)
	{
		PrintPolicy::WriteDouble(Stream, Value);
		return EJsonToken::Number;
	}

	EJsonToken WriteValueOnly(const FString& Value)
	{
		WriteStringValue(Value);
		return EJsonToken::String;
	}

	EJsonToken WriteValueOnly(const TCHAR* Value)
	{
		WriteStringValue(FString(Value));
		return EJsonToken::String;
	}

	/** A quoted string with JSON escapes; UTF-8 bytes pass through (UE: AppendEscapeJsonString). */
	void WriteStringValue(const FString& String)
	{
		FString Result("\"");
		for (const TCHAR Char : String)
		{
			switch (Char)
			{
				case '\\':
					Result += "\\\\";
					break;
				case '\n':
					Result += "\\n";
					break;
				case '\t':
					Result += "\\t";
					break;
				case '\b':
					Result += "\\b";
					break;
				case '\f':
					Result += "\\f";
					break;
				case '\r':
					Result += "\\r";
					break;
				case '"':
					Result += "\\\"";
					break;
				default:
					// Must escape control characters.
					if (uint8(Char) >= 32)
					{
						Result += Char;
					}
					else
					{
						Result += FString::Printf("\\u%04x", uint32(uint8(Char)));
					}
			}
		}
		Result += "\"";
		PrintPolicy::WriteString(Stream, Result);
	}

	FString* Stream;
	TArray<EJson> Stack;
	EJsonToken PreviousTokenWritten = EJsonToken::None;
	int32 IndentLevel;
};

/** Makes writers (UE: TJsonWriterFactory). */
template <class CharType = TCHAR, class PrintPolicy = TPrettyJsonPrintPolicy<CharType>>
class TJsonWriterFactory
{
public:
	static TSharedRef<TJsonWriter<CharType, PrintPolicy>> Create(FString* const Stream, int32 InitialIndent = 0)
	{
		return TJsonWriter<CharType, PrintPolicy>::Create(Stream, InitialIndent);
	}
};
