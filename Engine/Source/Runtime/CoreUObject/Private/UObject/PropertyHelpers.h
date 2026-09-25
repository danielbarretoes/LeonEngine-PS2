#pragma once

// Text import helpers shared by the property types (UE: FPropertyHelpers).

#include "CoreMinimal.h"

class FOutputDevice;

namespace UE::CoreUObject::Private
{
	/** Skips spaces and tabs. */
	const TCHAR* SkipWhitespace(const TCHAR* Buffer);

	/**
	 * Reads a token: a quoted string (with \" \\ \n \t escapes) or the text up to a delimiter (',' ')' whitespace).
	 * Returns the text after it, or nullptr on an unterminated string.
	 */
	const TCHAR* ReadToken(const TCHAR* Buffer, FString& OutToken, bool& bOutQuoted);

	/** Appends Value in quotes, escaping '"', '\\', newlines and tabs. */
	void AppendQuoted(FString& Out, const FString& Value);

	/** Writes an import error to ErrorText, or to the log when there is none. */
	void ReportImportError(FOutputDevice* ErrorText, const FString& Message);
} // namespace UE::CoreUObject::Private
