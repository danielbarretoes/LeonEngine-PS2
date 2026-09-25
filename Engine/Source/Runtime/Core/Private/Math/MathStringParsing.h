#pragma once

#include "CoreTypes.h"
#include "Misc/CString.h"

#include <cstdlib>

// Key=value parsing for the math InitFromString functions. FParse::Value replaces it in P4.

namespace MathStringParsing
{
	/** Finds Match (e.g. "X=") in Stream, ignoring case, and reads the float after it. */
	inline bool ParseFloat(const TCHAR* Stream, const TCHAR* Match, float& Value)
	{
		const TCHAR* Found = FCString::Stristr(Stream, Match);
		if (Found == nullptr)
		{
			return false;
		}
		Value = FCString::Atof(Found + FCString::Strlen(Match));
		return true;
	}

	inline bool ParseInt(const TCHAR* Stream, const TCHAR* Match, int32& Value)
	{
		const TCHAR* Found = FCString::Stristr(Stream, Match);
		if (Found == nullptr)
		{
			return false;
		}
		Value = FCString::Atoi(Found + FCString::Strlen(Match));
		return true;
	}

	/** Reads "A,B,C" into three floats; Stream advances past them. */
	inline bool ParseFloatTriple(const TCHAR*& Stream, float& A, float& B, float& C)
	{
		float* const Out[3] = {&A, &B, &C};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			TCHAR* End = nullptr;
			const float Parsed = std::strtof(Stream, &End);
			if (End == Stream)
			{
				return false;
			}
			*Out[Index] = Parsed;
			Stream = End;
			if (Index < 2)
			{
				if (*Stream != ',')
				{
					return false;
				}
				++Stream;
			}
		}
		return true;
	}
} // namespace MathStringParsing
