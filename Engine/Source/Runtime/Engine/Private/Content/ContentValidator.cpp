#include "Validation/ContentValidator.h"

#include "LeonMaterialFormat.h"
#include "Misc/Paths.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <system_error>

namespace
{

	constexpr int SupportedMaterialVersion = 1;

	bool IsNumberArray(const nlohmann::json& J, std::size_t MinSize)
	{
		if (!J.is_array() || J.size() < MinSize)
		{
			return false;
		}
		for (std::size_t I = 0; I < MinSize; ++I)
		{
			if (!J[I].is_number())
			{
				return false;
			}
		}
		return true;
	}

	void RequireVec3(
		FValidationReport& Report, const nlohmann::json& Parent, const char* Key, const std::string& InWhere)
	{
		if (!Parent.contains(Key))
		{
			return;
		}
		if (!IsNumberArray(Parent[Key], 3))
		{
			Report.Error(InWhere + "." + Key, "expected array of 3 numbers [x, y, z]");
		}
	}

	void RequireNumber(
		FValidationReport& Report, const nlohmann::json& Parent, const char* Key, const std::string& InWhere)
	{
		if (!Parent.contains(Key))
		{
			return;
		}
		if (!Parent[Key].is_number())
		{
			Report.Error(InWhere + "." + Key, "expected number");
		}
	}

	void RequireBool(
		FValidationReport& Report, const nlohmann::json& Parent, const char* Key, const std::string& InWhere)
	{
		if (!Parent.contains(Key))
		{
			return;
		}
		if (!Parent[Key].is_boolean())
		{
			Report.Error(InWhere + "." + Key, "expected boolean");
		}
	}

	void RequireString(
		FValidationReport& Report, const nlohmann::json& Parent, const char* Key, const std::string& InWhere)
	{
		if (!Parent.contains(Key))
		{
			return;
		}
		if (!Parent[Key].is_string())
		{
			Report.Error(InWhere + "." + Key, "expected string");
		}
	}

	void ValidateSurfaceFields(FValidationReport& Report, const nlohmann::json& Spec, const std::string& InWhere)
	{
		RequireVec3(Report, Spec, "albedo", InWhere);
		RequireVec3(Report, Spec, "specular", InWhere);
		RequireNumber(Report, Spec, "metallic", InWhere);
		RequireNumber(Report, Spec, "alpha", InWhere);
		RequireNumber(Report, Spec, "shininess", InWhere);
		RequireNumber(Report, Spec, "roughness", InWhere);
		RequireBool(Report, Spec, "unlit", InWhere);
		RequireBool(Report, Spec, "castsShadows", InWhere);
		RequireBool(Report, Spec, "planarMirror", InWhere);
		RequireString(Report, Spec, "albedoMap", InWhere);
		RequireString(Report, Spec, "normalMap", InWhere);

		if (Spec.contains("uvScale"))
		{
			const auto& Uv = Spec["uvScale"];
			if (!(Uv.is_number() || IsNumberArray(Uv, 2)))
			{
				Report.Error(InWhere + ".uvScale", "expected number or [u, v]");
			}
		}
		if (Spec.contains("tiling"))
		{
			const auto& Uv = Spec["tiling"];
			if (!(Uv.is_number() || IsNumberArray(Uv, 2)))
			{
				Report.Error(InWhere + ".tiling", "expected number or [u, v]");
			}
		}

		if (Spec.contains("albedoMap") && Spec["albedoMap"].is_string())
		{
			const std::string Key = Spec["albedoMap"].get<std::string>();
			if (Key != "checker")
			{
				const std::string Resolved = FPaths::ResolveAssetPath(Key);
				if (!std::filesystem::exists(Resolved))
				{
					Report.Warning(InWhere + ".albedoMap", "texture not found: " + Key);
				}
			}
		}
		if (Spec.contains("normalMap") && Spec["normalMap"].is_string())
		{
			const std::string Key = Spec["normalMap"].get<std::string>();
			if (Key != "bump")
			{
				const std::string Resolved = FPaths::ResolveAssetPath(Key);
				if (!std::filesystem::exists(Resolved))
				{
					Report.Warning(InWhere + ".normalMap", "texture not found: " + Key);
				}
			}
		}
	}

	/// True when the key resolves via pack-relative or global asset lookup.
	[[nodiscard]] bool LevelAssetExists(const std::string& LevelPath, const std::string& Key)
	{
		std::error_code Ec;
		const std::string Resolved = ResolveLevelAssetPath(LevelPath, Key);
		return !Resolved.empty() && std::filesystem::exists(Resolved, Ec) && !Ec;
	}

	void ValidateActorRecord(
		FValidationReport& Report, const FLevelActorRecord& Actor, std::size_t Index, const std::string& LevelPath)
	{
		const std::string LocalWhere = "actors[" + std::to_string(Index) + "]";

		if (Actor.ActorClass == ELevelActorClass::StaticMesh)
		{
			if (Actor.MeshPath.empty())
			{
				Report.Error(LocalWhere + ".mesh", "StaticMesh actor needs a mesh path");
			}
			else if (!LevelAssetExists(LevelPath, Actor.MeshPath))
			{
				Report.Warning(LocalWhere + ".mesh", "mesh file not found: " + Actor.MeshPath);
			}
		}
		else if (!Actor.MeshPath.empty())
		{
			Report.Error(LocalWhere + ".mesh", "only StaticMesh actors carry a mesh path");
		}

		if (Actor.ActorClass == ELevelActorClass::Sphere && (Actor.SphereSegments < 3 || Actor.SphereRings < 2))
		{
			Report.Error(LocalWhere, "sphere needs at least 3 segments and 2 rings");
		}

		if (Actor.ActorClass == ELevelActorClass::TriggerVolume && Actor.InteractRadius <= 0.0f)
		{
			Report.Error(LocalWhere + ".interactRadius", "expected a positive radius");
		}

		if (Actor.ActorClass == ELevelActorClass::PainCausingVolume)
		{
			if (Actor.DamagePerSecond < 0.0f)
			{
				Report.Error(LocalWhere + ".damagePerSecond", "expected a non-negative value");
			}
			if (Actor.DamageInterval <= 0.0f)
			{
				Report.Error(LocalWhere + ".damageInterval", "expected a positive interval");
			}
		}

		if (Actor.bHasFitHeight && Actor.FitHeight <= 0.0f)
		{
			Report.Error(LocalWhere + ".fitHeight", "expected a positive height");
		}

		if (!Actor.MaterialPath.empty())
		{
			const std::string ResolvedMat = ResolveLevelAssetPath(LevelPath, Actor.MaterialPath);
			std::error_code Ec;
			if (ResolvedMat.empty() || !std::filesystem::exists(ResolvedMat, Ec) || Ec)
			{
				Report.Error(LocalWhere + ".material", "material file not found: " + Actor.MaterialPath);
			}
			else
			{
				FValidationReport MatReport = ValidateMaterialFile(ResolvedMat);
				for (FValidationIssue& Issue : MatReport.Issues)
				{
					Issue.Where = LocalWhere + ".material->" + Issue.Where;
					Report.Issues.push_back(std::move(Issue));
				}
			}
		}
	}

	void ValidateLightRecord(FValidationReport& Report, const FLevelLightRecord& Light, std::size_t Index)
	{
		const std::string LocalWhere = "lights[" + std::to_string(Index) + "]";
		if (Light.Intensity < 0.0f)
		{
			Report.Error(LocalWhere + ".intensity", "expected a non-negative value");
		}
		if (Light.LightClass == ELevelLightClass::PointLight && Light.Range <= 0.0f)
		{
			Report.Error(LocalWhere + ".range", "point light range must be positive");
		}
	}

} // namespace

void FValidationReport::Error(std::string InWhere, std::string InMessage)
{
	Issues.push_back(FValidationIssue{EValidationSeverity::Error, std::move(InWhere), std::move(InMessage)});
}

void FValidationReport::Warning(std::string InWhere, std::string InMessage)
{
	Issues.push_back(FValidationIssue{EValidationSeverity::Warning, std::move(InWhere), std::move(InMessage)});
}

bool FValidationReport::Ok() const
{
	return ErrorCount() == 0;
}

std::size_t FValidationReport::ErrorCount() const
{
	std::size_t N = 0;
	for (const FValidationIssue& Issue : Issues)
	{
		if (Issue.Severity == EValidationSeverity::Error)
		{
			++N;
		}
	}
	return N;
}

std::size_t FValidationReport::WarningCount() const
{
	return Issues.size() - ErrorCount();
}

void FValidationReport::LogToStderr() const
{
	const char* Label = SourcePath.empty() ? "<json>" : SourcePath.c_str();
	for (const FValidationIssue& Issue : Issues)
	{
		const char* Kind = Issue.Severity == EValidationSeverity::Error ? "error" : "warning";
		std::cerr << "ContentValidator: " << Kind << " in " << Label;
		if (!Issue.Where.empty())
		{
			std::cerr << " @ " << Issue.Where;
		}
		std::cerr << ": " << Issue.Message << '\n';
	}
	if (!Ok())
	{
		std::cerr << "ContentValidator: " << ErrorCount() << " error(s), " << WarningCount() << " warning(s) in "
				  << Label << '\n';
	}
	else if (WarningCount() > 0)
	{
		std::cerr << "ContentValidator: OK with " << WarningCount() << " warning(s) in " << Label << '\n';
	}
}

FValidationReport ValidateMaterialDocument(const nlohmann::json& Doc, const std::string& InSourcePath)
{
	// Deprecated JSON material path — keep for callers that still pass JSON.
	FValidationReport Report;
	Report.SourcePath = InSourcePath;

	if (!Doc.is_object())
	{
		Report.Error("", "material root must be a JSON object");
		return Report;
	}

	if (Doc.contains("version"))
	{
		if (!Doc["version"].is_number())
		{
			Report.Error("version", "expected number");
		}
		else
		{
			int Version = 0;
			if (Doc["version"].is_number_integer())
			{
				Version = Doc["version"].get<int>();
			}
			else
			{
				const double Raw = Doc["version"].get<double>();
				if (std::floor(Raw) != Raw)
				{
					Report.Error("version", "must be a whole number");
				}
				else
				{
					Version = static_cast<int>(Raw);
				}
			}
			if (Version != SupportedMaterialVersion)
			{
				Report.Error("version",
					"unsupported material version (expected " + std::to_string(SupportedMaterialVersion) + ")");
			}
		}
	}

	RequireString(Report, Doc, "name", "");
	ValidateSurfaceFields(Report, Doc, "");
	return Report;
}

FValidationReport ValidateMaterialFile(const std::string& Path)
{
	FValidationReport Report;
	Report.SourcePath = Path;

	const auto ExtPos = Path.find_last_of('.');
	std::string Ext = ExtPos == std::string::npos ? std::string{} : Path.substr(ExtPos);
	for (char& C : Ext)
	{
		C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
	}
	if (Ext != ".lmat")
	{
		Report.Error("", "material asset must be .lmat");
		return Report;
	}

	FLeonMaterialDocument Doc;
	if (!LoadLeonMaterialDocument(Path, Doc))
	{
		Report.Error("", "failed to load .lmat");
		return Report;
	}

	if (Doc.Material.Metallic < 0.0f || Doc.Material.Metallic > 1.0f)
	{
		Report.Warning("Metallic", "expected value in [0, 1]");
	}
	if (Doc.Material.Roughness < 0.0f || Doc.Material.Roughness > 1.0f)
	{
		Report.Warning("Roughness", "expected value in [0, 1]");
	}

	auto WarnMissingMap = [&](const std::string& MapPath, const char* InWhere)
	{
		if (MapPath.empty() || MapPath == "checker" || MapPath == "bump")
		{
			return;
		}
		const std::string Resolved = FPaths::ResolveAssetPath(MapPath);
		std::error_code Ec;
		if (Resolved.empty() || !std::filesystem::exists(Resolved, Ec) || Ec)
		{
			Report.Warning(InWhere, "texture not found: " + MapPath);
		}
	};
	WarnMissingMap(Doc.BaseColorMapPath, "BaseColorMap");
	WarnMissingMap(Doc.NormalMapPath, "NormalMap");
	return Report;
}

FValidationReport ValidateLevelDocument(const FLevelDocument& Doc, const std::string& InSourcePath)
{
	FValidationReport Report;
	Report.SourcePath = InSourcePath;

	// Magic / version / class enums are already enforced by the `.llev` reader; an empty
	// actor list is valid (blank / lights-only levels).

	for (std::size_t I = 0; I < Doc.Actors.size(); ++I)
	{
		ValidateActorRecord(Report, Doc.Actors[I], I, InSourcePath);
	}
	for (std::size_t I = 0; I < Doc.Lights.size(); ++I)
	{
		ValidateLightRecord(Report, Doc.Lights[I], I);
	}

	return Report;
}
