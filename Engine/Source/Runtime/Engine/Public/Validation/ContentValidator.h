#pragma once

#include "Level/LeonLevelFormat.h"

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <vector>

enum class EValidationSeverity
{
	Error,
	Warning,
};

struct ENGINE_API FValidationIssue
{
	EValidationSeverity Severity = EValidationSeverity::Error;
	std::string Where; // e.g. "actors[2].material" or "albedo"
	std::string Message;
};

/// Collects validation issues for level documents / material assets.
struct ENGINE_API FValidationReport
{
	std::string SourcePath;
	std::vector<FValidationIssue> Issues;

	void Error(std::string InWhere, std::string InMessage);
	void Warning(std::string InWhere, std::string InMessage);

	[[nodiscard]] bool Ok() const;
	[[nodiscard]] std::size_t ErrorCount() const;
	[[nodiscard]] std::size_t WarningCount() const;

	/// Print to stderr (Errors / Warnings with source path).
	void LogToStderr() const;
};

/// Validate a decoded `.llev` document (referenced material / mesh assets, value ranges).
/// Magic and version are already enforced by the `.llev` reader.
[[nodiscard]] FValidationReport ValidateLevelDocument(const FLevelDocument& Doc, const std::string& InSourcePath);

/// Validate a parsed material JSON document.
/// Deprecated: prefer `.lmat` via `ValidateMaterialFile` / `LoadLeonMaterialDocument`.
/// Still used only when callers pass a JSON document explicitly.
[[nodiscard]] FValidationReport ValidateMaterialDocument(const nlohmann::json& Doc, const std::string& InSourcePath);

/// Open + parse + validate a `.lmat` material file (does not upload GPU resources).
[[nodiscard]] FValidationReport ValidateMaterialFile(const std::string& Path);
