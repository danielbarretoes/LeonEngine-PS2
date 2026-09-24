#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "Validation/ContentValidator.h"
#include "Misc/Paths.h"
#include "LeonMaterialFormat.h"
#include <nlohmann/json.hpp>
#include <system_error>

namespace {

constexpr int kSupportedMaterialVersion = 1;

bool isNumberArray(const nlohmann::json& j, std::size_t minSize) {
    if (!j.is_array() || j.size() < minSize) {
        return false;
    }
    for (std::size_t i = 0; i < minSize; ++i) {
        if (!j[i].is_number()) {
            return false;
        }
    }
    return true;
}

void requireVec3(ValidationReport& report, const nlohmann::json& parent, const char* key,
                 const std::string& where) {
    if (!parent.contains(key)) {
        return;
    }
    if (!isNumberArray(parent[key], 3)) {
        report.error(where + "." + key, "expected array of 3 numbers [x, y, z]");
    }
}

void requireNumber(ValidationReport& report, const nlohmann::json& parent, const char* key,
                   const std::string& where) {
    if (!parent.contains(key)) {
        return;
    }
    if (!parent[key].is_number()) {
        report.error(where + "." + key, "expected number");
    }
}

void requireBool(ValidationReport& report, const nlohmann::json& parent, const char* key,
                 const std::string& where) {
    if (!parent.contains(key)) {
        return;
    }
    if (!parent[key].is_boolean()) {
        report.error(where + "." + key, "expected boolean");
    }
}

void requireString(ValidationReport& report, const nlohmann::json& parent, const char* key,
                   const std::string& where) {
    if (!parent.contains(key)) {
        return;
    }
    if (!parent[key].is_string()) {
        report.error(where + "." + key, "expected string");
    }
}

void validateSurfaceFields(ValidationReport& report, const nlohmann::json& spec,
                           const std::string& where) {
    requireVec3(report, spec, "albedo", where);
    requireVec3(report, spec, "specular", where);
    requireNumber(report, spec, "metallic", where);
    requireNumber(report, spec, "alpha", where);
    requireNumber(report, spec, "shininess", where);
    requireNumber(report, spec, "roughness", where);
    requireBool(report, spec, "unlit", where);
    requireBool(report, spec, "castsShadows", where);
    requireBool(report, spec, "planarMirror", where);
    requireString(report, spec, "albedoMap", where);
    requireString(report, spec, "normalMap", where);

    if (spec.contains("uvScale")) {
        const auto& uv = spec["uvScale"];
        if (!(uv.is_number() || isNumberArray(uv, 2))) {
            report.error(where + ".uvScale", "expected number or [u, v]");
        }
    }
    if (spec.contains("tiling")) {
        const auto& uv = spec["tiling"];
        if (!(uv.is_number() || isNumberArray(uv, 2))) {
            report.error(where + ".tiling", "expected number or [u, v]");
        }
    }

    if (spec.contains("albedoMap") && spec["albedoMap"].is_string()) {
        const std::string key = spec["albedoMap"].get<std::string>();
        if (key != "checker") {
            const std::string resolved = FPaths::ResolveAssetPath(key);
            if (!std::filesystem::exists(resolved)) {
                report.warning(where + ".albedoMap", "texture not found: " + key);
            }
        }
    }
    if (spec.contains("normalMap") && spec["normalMap"].is_string()) {
        const std::string key = spec["normalMap"].get<std::string>();
        if (key != "bump") {
            const std::string resolved = FPaths::ResolveAssetPath(key);
            if (!std::filesystem::exists(resolved)) {
                report.warning(where + ".normalMap", "texture not found: " + key);
            }
        }
    }
}

/// True when the key resolves via pack-relative or global asset lookup.
[[nodiscard]] bool LevelAssetExists(const std::string& levelPath, const std::string& key) {
    std::error_code ec;
    const std::string resolved = ResolveLevelAssetPath(levelPath, key);
    return !resolved.empty() && std::filesystem::exists(resolved, ec) && !ec;
}

void validateActorRecord(ValidationReport& report, const LevelActorRecord& actor,
                         std::size_t index, const std::string& levelPath) {
    const std::string where = "actors[" + std::to_string(index) + "]";

    if (actor.actorClass == ELevelActorClass::StaticMesh) {
        if (actor.meshPath.empty()) {
            report.error(where + ".mesh", "StaticMesh actor needs a mesh path");
        } else if (!LevelAssetExists(levelPath, actor.meshPath)) {
            report.warning(where + ".mesh", "mesh file not found: " + actor.meshPath);
        }
    } else if (!actor.meshPath.empty()) {
        report.error(where + ".mesh", "only StaticMesh actors carry a mesh path");
    }

    if (actor.actorClass == ELevelActorClass::Sphere &&
        (actor.sphereSegments < 3 || actor.sphereRings < 2)) {
        report.error(where, "sphere needs at least 3 segments and 2 rings");
    }

    if (actor.actorClass == ELevelActorClass::TriggerVolume && actor.interactRadius <= 0.0f) {
        report.error(where + ".interactRadius", "expected a positive radius");
    }

    if (actor.actorClass == ELevelActorClass::PainCausingVolume) {
        if (actor.damagePerSecond < 0.0f) {
            report.error(where + ".damagePerSecond", "expected a non-negative value");
        }
        if (actor.damageInterval <= 0.0f) {
            report.error(where + ".damageInterval", "expected a positive interval");
        }
    }

    if (actor.hasFitHeight && actor.fitHeight <= 0.0f) {
        report.error(where + ".fitHeight", "expected a positive height");
    }

    if (!actor.materialPath.empty()) {
        const std::string resolvedMat = ResolveLevelAssetPath(levelPath, actor.materialPath);
        std::error_code ec;
        if (resolvedMat.empty() || !std::filesystem::exists(resolvedMat, ec) || ec) {
            report.error(where + ".material", "material file not found: " + actor.materialPath);
        } else {
            ValidationReport matReport = ValidateMaterialFile(resolvedMat);
            for (ValidationIssue& issue : matReport.issues) {
                issue.where = where + ".material->" + issue.where;
                report.issues.push_back(std::move(issue));
            }
        }
    }

    if (!actor.lightmapPath.empty() && !LevelAssetExists(levelPath, actor.lightmapPath)) {
        report.warning(where + ".lightmap", "lightmap file not found: " + actor.lightmapPath);
    }
}

void validateLightRecord(ValidationReport& report, const LevelLightRecord& light,
                         std::size_t index) {
    const std::string where = "lights[" + std::to_string(index) + "]";
    if (light.intensity < 0.0f) {
        report.error(where + ".intensity", "expected a non-negative value");
    }
    if (light.lightClass == ELevelLightClass::PointLight && light.range <= 0.0f) {
        report.error(where + ".range", "point light range must be positive");
    }
}

} // namespace

void ValidationReport::error(std::string where, std::string message) {
    issues.push_back(
        ValidationIssue{EValidationSeverity::Error, std::move(where), std::move(message)});
}

void ValidationReport::warning(std::string where, std::string message) {
    issues.push_back(
        ValidationIssue{EValidationSeverity::Warning, std::move(where), std::move(message)});
}

bool ValidationReport::ok() const {
    return errorCount() == 0;
}

std::size_t ValidationReport::errorCount() const {
    std::size_t n = 0;
    for (const ValidationIssue& issue : issues) {
        if (issue.severity == EValidationSeverity::Error) {
            ++n;
        }
    }
    return n;
}

std::size_t ValidationReport::warningCount() const {
    return issues.size() - errorCount();
}

void ValidationReport::logToStderr() const {
    const char* label = sourcePath.empty() ? "<json>" : sourcePath.c_str();
    for (const ValidationIssue& issue : issues) {
        const char* kind = issue.severity == EValidationSeverity::Error ? "error" : "warning";
        std::cerr << "ContentValidator: " << kind << " in " << label;
        if (!issue.where.empty()) {
            std::cerr << " @ " << issue.where;
        }
        std::cerr << ": " << issue.message << '\n';
    }
    if (!ok()) {
        std::cerr << "ContentValidator: " << errorCount() << " error(s), " << warningCount()
                  << " warning(s) in " << label << '\n';
    } else if (warningCount() > 0) {
        std::cerr << "ContentValidator: OK with " << warningCount() << " warning(s) in " << label
                  << '\n';
    }
}

ValidationReport ValidateMaterialDocument(const nlohmann::json& doc,
                                          const std::string& sourcePath) {
    // Deprecated JSON material path — keep for callers that still pass JSON.
    ValidationReport report;
    report.sourcePath = sourcePath;

    if (!doc.is_object()) {
        report.error("", "material root must be a JSON object");
        return report;
    }

    if (doc.contains("version")) {
        if (!doc["version"].is_number()) {
            report.error("version", "expected number");
        } else {
            int version = 0;
            if (doc["version"].is_number_integer()) {
                version = doc["version"].get<int>();
            } else {
                const double raw = doc["version"].get<double>();
                if (std::floor(raw) != raw) {
                    report.error("version", "must be a whole number");
                } else {
                    version = static_cast<int>(raw);
                }
            }
            if (version != kSupportedMaterialVersion) {
                report.error("version", "unsupported material version (expected " +
                                            std::to_string(kSupportedMaterialVersion) + ")");
            }
        }
    }

    requireString(report, doc, "name", "");
    validateSurfaceFields(report, doc, "");
    return report;
}

ValidationReport ValidateMaterialFile(const std::string& path) {
    ValidationReport report;
    report.sourcePath = path;

    const auto extPos = path.find_last_of('.');
    std::string ext = extPos == std::string::npos ? std::string{} : path.substr(extPos);
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (ext != ".lmat") {
        report.error("", "material asset must be .lmat");
        return report;
    }

    LeonMaterialDocument doc;
    if (!LoadLeonMaterialDocument(path, doc)) {
        report.error("", "failed to load .lmat");
        return report;
    }

    if (doc.material.metallic < 0.0f || doc.material.metallic > 1.0f) {
        report.warning("Metallic", "expected value in [0, 1]");
    }
    if (doc.material.roughness < 0.0f || doc.material.roughness > 1.0f) {
        report.warning("Roughness", "expected value in [0, 1]");
    }

    auto warnMissingMap = [&](const std::string& mapPath, const char* where) {
        if (mapPath.empty() || mapPath == "checker" || mapPath == "bump") {
            return;
        }
        const std::string resolved = FPaths::ResolveAssetPath(mapPath);
        std::error_code ec;
        if (resolved.empty() || !std::filesystem::exists(resolved, ec) || ec) {
            report.warning(where, "texture not found: " + mapPath);
        }
    };
    warnMissingMap(doc.baseColorMapPath, "BaseColorMap");
    warnMissingMap(doc.normalMapPath, "NormalMap");
    return report;
}

ValidationReport ValidateLevelDocument(const LevelDocument& doc, const std::string& sourcePath) {
    ValidationReport report;
    report.sourcePath = sourcePath;

    // Magic / version / class enums are already enforced by the `.llev` reader; an empty
    // actor list is valid (blank / lights-only levels).
    if (!doc.environmentPath.empty()) {
        std::error_code ec;
        if (!std::filesystem::exists(FPaths::ResolveAssetPath(doc.environmentPath), ec) || ec) {
            report.warning("environment", "HDR file not found: " + doc.environmentPath);
        }
    }
    if (doc.environmentExposure < 0.0f) {
        report.error("environmentExposure", "expected a non-negative value");
    }

    for (std::size_t i = 0; i < doc.actors.size(); ++i) {
        validateActorRecord(report, doc.actors[i], i, sourcePath);
    }
    for (std::size_t i = 0; i < doc.lights.size(); ++i) {
        validateLightRecord(report, doc.lights[i], i);
    }

    return report;
}

