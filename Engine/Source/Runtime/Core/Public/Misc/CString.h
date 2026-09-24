#pragma once

#include <cctype>
#include <string>
#include <string_view>

/** Character string helpers (UE: FCString, reduced to what the engine uses). */
struct FCString
{
	/** ASCII lower-case copy (case-insensitive JSON class / primitive names). */
	static std::string ToLower(std::string_view Text)
	{
		std::string Result(Text);
		for (char& Character : Result)
		{
			Character = static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
		}
		return Result;
	}
};
