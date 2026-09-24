#pragma once

#include "CoreTypes.h"

#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>

#include <string>

/** nlohmann::json helpers shared by the level / asset loaders (UE: FJsonSerializer helpers). */
struct JSON_API FJsonUtils
{
	/** Reads a JSON array of 3 numbers, or returns Fallback when missing / invalid. */
	[[nodiscard]] static glm::vec3 ReadVec3(const nlohmann::json& Json, const glm::vec3& Fallback);

	/** Parses a JSON file into OutJson. Returns false on I/O or parse error (logged to stderr). */
	[[nodiscard]] static bool LoadJsonFile(const std::string& Path, nlohmann::json& OutJson);
};
