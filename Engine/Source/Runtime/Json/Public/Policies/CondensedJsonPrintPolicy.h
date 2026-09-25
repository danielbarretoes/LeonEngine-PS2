#pragma once

#include "CoreTypes.h"
#include "Policies/JsonPrintPolicy.h"

/** No whitespace at all (UE: TCondensedJsonPrintPolicy). */
template <class CharType = TCHAR>
struct TCondensedJsonPrintPolicy : public TJsonPrintPolicy<CharType>
{
	static inline void WriteLineTerminator(FString* /*Stream*/)
	{
	}

	static inline void WriteTabs(FString* /*Stream*/, int32 /*Count*/)
	{
	}

	static inline void WriteSpace(FString* /*Stream*/)
	{
	}
};
