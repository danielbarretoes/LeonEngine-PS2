#pragma once

#include "CoreTypes.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

/** File read / write helpers (UE: FFileHelper). */
struct CORE_API FFileHelper
{
	/** Writes bytes via a same-directory temp file + rename (crash-safe vs truncate-in-place). */
	[[nodiscard]] static bool WriteFileAtomic(const std::filesystem::path& Path, const void* Data, std::size_t Size);

	[[nodiscard]] static bool WriteFileAtomic(const std::filesystem::path& Path, const std::vector<std::uint8_t>& Bytes);

	[[nodiscard]] static bool WriteTextFileAtomic(const std::filesystem::path& Path, std::string_view Text);
};
