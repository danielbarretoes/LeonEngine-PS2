#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"

/**
 * The process command line without the executable name (UE: FCommandLine). Query it with FParse:
 * FParse::Param(FCommandLine::Get(), "nullrhi"), FParse::Value(FCommandLine::Get(), "map=", MapName).
 * Stored in an FString instead of UE's fixed 16 KB buffers.
 */
struct CORE_API FCommandLine
{
	static bool IsInitialized();

	/** The current command line; empty until Set. */
	static const TCHAR* Get();

	/** The command line as first set, before Append. */
	static const TCHAR* GetOriginal();

	/** Replaces the command line; the first call also sets the original (UE: Set). */
	static bool Set(const TCHAR* NewCommandLine);

	/** Appends text as is: add a leading space for a new switch (UE: Append). */
	static void Append(const TCHAR* AppendString);

	/**
	 * Joins argv[1..] into a command line; an argument with spaces is quoted, after the '=' for "Key=Value"
	 * (UE: BuildFromArgV).
	 */
	static FString BuildFromArgV(const TCHAR* Prefix, int32 ArgC, TCHAR* ArgV[], const TCHAR* Suffix);

	/** Splits into tokens and switches (switches without their leading '-') (UE: Parse). */
	static void Parse(const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches);
};
