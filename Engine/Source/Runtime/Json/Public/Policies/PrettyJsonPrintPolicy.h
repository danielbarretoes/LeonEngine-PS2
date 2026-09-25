#pragma once

#include "CoreTypes.h"
#include "Policies/JsonPrintPolicy.h"

/** Tabs and line breaks, like UE's saved descriptors (UE: TPrettyJsonPrintPolicy). */
template <class CharType = TCHAR>
struct TPrettyJsonPrintPolicy : public TJsonPrintPolicy<CharType>
{
	static inline void WriteLineTerminator(FString* Stream)
	{
		Stream->Append(LINE_TERMINATOR);
	}

	static inline void WriteTabs(FString* Stream, int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Stream->AppendChar('\t');
		}
	}

	static inline void WriteSpace(FString* Stream)
	{
		Stream->AppendChar(' ');
	}
};
