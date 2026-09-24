#pragma once

#include <cctype>
#include <string>
#include <string_view>

/** Character string helpers (UE: FCString, reduced to what the engine uses). */
struct CORE_API FCString
{
	/** ASCII lower-case copy (case-insensitive JSON class / primitive names). */
	static std::string ToLower(std::string_view Text)
	{
		std::string Result(Text);
		for (char& Char : Result)
		{
			Char = static_cast<char>(std::tolower(static_cast<unsigned char>(Char)));
		}
		return Result;
	}
};
