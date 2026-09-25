#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Misc/CString.h"

/**
 * Base print policy: how characters and numbers go into the output string (UE: TJsonPrintPolicy). Numbers use the
 * shortest "%.Ng" form that reads back to the same value (UE always writes "%.17g").
 */
template <class CharType = TCHAR>
struct TJsonPrintPolicy
{
	static inline void WriteChar(FString* Stream, CharType Char)
	{
		Stream->AppendChar(Char);
	}

	static inline void WriteString(FString* Stream, const FString& String)
	{
		Stream->Append(String);
	}

	static inline void WriteFloat(FString* Stream, float Value)
	{
		WriteDouble(Stream, double(Value));
	}

	static inline void WriteDouble(FString* Stream, double Value)
	{
		FString Text;
		for (int32 Precision = 15; Precision <= 17; ++Precision)
		{
			Text = FString::Printf("%.*g", Precision, Value);
			if (FCString::Atod(*Text) == Value)
			{
				break;
			}
		}
		Stream->Append(Text);
	}
};
