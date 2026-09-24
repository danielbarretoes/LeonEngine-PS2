#pragma once

#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>
#include <string>


/// Read a JSON array of 3 numbers, or return `fallback` if missing/invalid.
[[nodiscard]] glm::vec3 ReadVec3(const nlohmann::json& j, const glm::vec3& fallback);

/// Parse a JSON file into `out`. Returns false on I/O or parse error (logs to stderr).
[[nodiscard]] bool LoadJsonFile(const std::string& path, nlohmann::json& out);

