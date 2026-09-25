#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "UObject/NameTypes.h"

struct FGuid;

/**
 * Parsing of "Key=Value" streams, command lines and tokens (UE: FParse). A match only counts where it starts a word:
 * "X=" does not match inside "MAX=".
 */
struct CORE_API FParse
{
	/** Value after Match; a quoted value may hold spaces, an unquoted one ends at whitespace (and ',' / ')'). */
	static bool Value(const TCHAR* Stream, const TCHAR* Match, FString& Value, bool bShouldStopOnSeparator = true);
	static bool Value(
		const TCHAR* Stream, const TCHAR* Match, TCHAR* Value, int32 MaxLen, bool bShouldStopOnSeparator = true);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, FName& Name);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, uint8& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, int8& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, uint16& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, int16& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, uint32& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, int32& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, uint64& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, int64& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, float& Value);
	static bool Value(const TCHAR* Stream, const TCHAR* Match, FGuid& Guid);

	/** Value after Match read as a boolean ("True", "Yes", "On", non-zero) (UE: FParse::Bool). */
	static bool Bool(const TCHAR* Stream, const TCHAR* Match, bool& OnOff);

	/** "-Param" or "/Param" as a whole switch (UE: FParse::Param). */
	static bool Param(const TCHAR* Stream, const TCHAR* Param);

	/** Skips Match at the start of Stream when it is a whole word (UE: FParse::Command). */
	static bool Command(const TCHAR** Stream, const TCHAR* Match, bool bParseMightTriggerExecution = true);

	/** Next whitespace-separated token; a quoted token may hold spaces (UE: FParse::Token). */
	static bool Token(const TCHAR*& Str, TCHAR* Result, int32 MaxLen, bool bUseEscape);
	static bool Token(const TCHAR*& Str, FString& Arg, bool bUseEscape);
	static FString Token(const TCHAR*& Str, bool bUseEscape);

	/** Next token of letters, digits and '_' (UE: FParse::AlnumToken). */
	static bool AlnumToken(const TCHAR*& Str, FString& Arg);

	/** Next line; unless Exact, "//" starts a comment and '|' ends the line (UE: FParse::Line). */
	static bool Line(const TCHAR** Stream, FString& Result, bool bExact = false);

	/** A "quoted string" with C escapes; \u and \x values are written as UTF-8 (UE: FParse::QuotedString). */
	static bool QuotedString(const TCHAR* Buffer, FString& Value, int32* OutNumCharsRead = nullptr);

	/** Value of a hexadecimal digit, 0 for anything else (UE: FParse::HexDigit). */
	static int32 HexDigit(TCHAR C);

	/** Value of a run of hexadecimal digits (UE: FParse::HexNumber). */
	static uint32 HexNumber(const TCHAR* HexString);
	static uint64 HexNumber64(const TCHAR* HexString);

	/** Skips whitespace, blank lines and "//" / ";" comments (UE: FParse::Next). */
	static void Next(const TCHAR** Stream);
};
