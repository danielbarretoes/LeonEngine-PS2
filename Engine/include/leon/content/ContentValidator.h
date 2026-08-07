#pragma once

#include <leon/level/LeonLevelFormat.h>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace leon {

enum class EValidationSeverity {
    Error,
    Warning,
};

struct ValidationIssue {
    EValidationSeverity severity = EValidationSeverity::Error;
    std::string where; // e.g. "actors[2].material" or "albedo"
    std::string message;
};

/// Collects validation issues for level documents / material assets.
struct ValidationReport {
    std::string sourcePath;
    std::vector<ValidationIssue> issues;

    void error(std::string where, std::string message);
    void warning(std::string where, std::string message);

    [[nodiscard]] bool ok() const;
    [[nodiscard]] std::size_t errorCount() const;
    [[nodiscard]] std::size_t warningCount() const;

    /// Print to stderr (Errors / Warnings with source path).
    void logToStderr() const;
};

/// Validate a decoded `.llev` document (referenced material / mesh assets, value ranges).
/// Magic and version are already enforced by the `.llev` reader.
[[nodiscard]] ValidationReport ValidateLevelDocument(const LevelDocument& doc,
                                                     const std::string& sourcePath);

/// Validate a parsed material JSON document.
/// Deprecated: prefer `.lmat` via `ValidateMaterialFile` / `LoadLeonMaterialDocument`.
/// Still used only when callers pass a JSON document explicitly.
[[nodiscard]] ValidationReport ValidateMaterialDocument(const nlohmann::json& doc,
                                                        const std::string& sourcePath);

/// Open + parse + validate a `.lmat` material file (does not upload GPU resources).
[[nodiscard]] ValidationReport ValidateMaterialFile(const std::string& path);

} // namespace leon
