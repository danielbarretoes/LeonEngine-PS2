#pragma once

#include "CoreTypes.h"

// String conversions (UE: Containers/StringConv.h). TCHAR is UTF-8, so the UTF-8 / ANSI macros are the identity;
// wide strings (UTF-16 on Windows, UTF-32 elsewhere) are converted. The converted pointer lives until the end of the
// full expression: SomeWin32Api(TCHAR_TO_WCHAR(*Path)).

/** UTF-8 TCHAR string -> wide string (invalid UTF-8 becomes U+FFFD). */
class CORE_API FTCHARToWide
{
public:
	explicit FTCHARToWide(const TCHAR* Source);
	FTCHARToWide(const TCHAR* Source, int32 SourceLength);
	~FTCHARToWide();

	FTCHARToWide(const FTCHARToWide&) = delete;
	FTCHARToWide& operator=(const FTCHARToWide&) = delete;

	FORCEINLINE const WIDECHAR* Get() const
	{
		return Buffer;
	}

	/** Converted length in WIDECHARs, without the terminator. */
	FORCEINLINE int32 Length() const
	{
		return ConvertedLength;
	}

private:
	void Convert(const TCHAR* Source, int32 SourceLength);

	WIDECHAR InlineBuffer[128];
	WIDECHAR* Buffer = InlineBuffer;
	int32 ConvertedLength = 0;
};

/** Wide string -> UTF-8 TCHAR string. */
class CORE_API FWideToTCHAR
{
public:
	explicit FWideToTCHAR(const WIDECHAR* Source);
	FWideToTCHAR(const WIDECHAR* Source, int32 SourceLength);
	~FWideToTCHAR();

	FWideToTCHAR(const FWideToTCHAR&) = delete;
	FWideToTCHAR& operator=(const FWideToTCHAR&) = delete;

	FORCEINLINE const TCHAR* Get() const
	{
		return Buffer;
	}

	/** Converted length in TCHARs, without the terminator. */
	FORCEINLINE int32 Length() const
	{
		return ConvertedLength;
	}

private:
	void Convert(const WIDECHAR* Source, int32 SourceLength);

	TCHAR InlineBuffer[256];
	TCHAR* Buffer = InlineBuffer;
	int32 ConvertedLength = 0;
};

#define TCHAR_TO_WCHAR(Str) (FTCHARToWide(Str).Get())
#define WCHAR_TO_TCHAR(Str) (FWideToTCHAR(Str).Get())

// Identity: TCHAR already is UTF-8 / ANSI.
#define TCHAR_TO_UTF8(Str) ((const ANSICHAR*)(Str))
#define UTF8_TO_TCHAR(Str) ((const TCHAR*)(Str))
#define TCHAR_TO_ANSI(Str) ((const ANSICHAR*)(Str))
#define ANSI_TO_TCHAR(Str) ((const TCHAR*)(Str))
