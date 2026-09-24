#include "MaterialAsset.h"

#include "LeonMaterialFormat.h"
#include "Misc/Paths.h"
#include "ResourceCache.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <iostream>

namespace
{

	glm::vec3 ReadVec3(const nlohmann::json& J, const glm::vec3& Fallback)
	{
		if (!J.is_array() || J.size() < 3)
		{
			return Fallback;
		}
		return {J[0].get<float>(), J[1].get<float>(), J[2].get<float>()};
	}

	void ApplyMaterialMaps(FResourceCache& Resources, FMaterial& Material, const nlohmann::json& Object)
	{
		if (Object.contains("albedoMap") && Object["albedoMap"].is_string())
		{
			const std::string Key = Object["albedoMap"].get<std::string>();
			if (Key == "checker")
			{
				Material.AlbedoMap = Resources.CheckerTexture(64);
			}
			else
			{
				Material.AlbedoMap = Resources.LoadTexture(FPaths::ResolveAssetPath(Key));
			}
		}
		if (Object.contains("normalMap") && Object["normalMap"].is_string())
		{
			const std::string Key = Object["normalMap"].get<std::string>();
			if (Key == "bump")
			{
				Material.NormalMap = Resources.BumpNormalTexture(256);
			}
			else
			{
				Material.NormalMap = Resources.LoadTexture(FPaths::ResolveAssetPath(Key));
			}
		}
	}

} // namespace

bool HasMaterialSurfaceFields(const nlohmann::json& Spec)
{
	return Spec.contains("albedo") || Spec.contains("alpha") || Spec.contains("specular") ||
		Spec.contains("metallic") || Spec.contains("shininess") || Spec.contains("roughness") ||
		Spec.contains("unlit") || Spec.contains("albedoMap") || Spec.contains("normalMap") ||
		Spec.contains("uvScale") || Spec.contains("tiling");
}

void PatchMaterialFromJson(FResourceCache& Resources, FMaterial& Material, const nlohmann::json& Spec)
{
	if (Spec.contains("unlit") && Spec["unlit"].is_boolean() && Spec["unlit"].get<bool>())
	{
		Material.Shading = EMaterialShadingModel::Unlit;
	}
	if (Spec.contains("albedo"))
	{
		Material.Albedo = ReadVec3(Spec["albedo"], Material.Albedo);
	}
	if (Spec.contains("specular"))
	{
		Material.Specular = ReadVec3(Spec["specular"], Material.Specular);
	}
	if (Spec.contains("metallic"))
	{
		Material.Metallic = Spec.value("metallic", Material.Metallic);
	}
	if (Spec.contains("alpha"))
	{
		Material.Alpha = Spec.value("alpha", Material.Alpha);
	}
	if (Spec.contains("shininess"))
	{
		Material.Shininess = Spec.value("shininess", Material.Shininess);
		if (!Spec.contains("roughness"))
		{
			Material.SyncRoughnessFromShininess();
		}
	}
	if (Spec.contains("roughness"))
	{
		Material.Roughness = std::clamp(Spec.value("roughness", Material.Roughness), 0.04f, 1.0f);
	}
	const nlohmann::json* UvNode = nullptr;
	if (Spec.contains("uvScale"))
	{
		UvNode = &Spec["uvScale"];
	}
	else if (Spec.contains("tiling"))
	{
		UvNode = &Spec["tiling"];
	}
	if (UvNode != nullptr)
	{
		if (UvNode->is_number())
		{
			const float S = UvNode->get<float>();
			Material.UvScale = {S, S};
		}
		else if (UvNode->is_array() && UvNode->size() >= 2)
		{
			Material.UvScale = {(*UvNode)[0].get<float>(), (*UvNode)[1].get<float>()};
		}
	}
	if (Spec.contains("castsShadows"))
	{
		Material.bCastsShadows = Spec.value("castsShadows", Material.bCastsShadows);
	}
	if (Spec.contains("planarMirror"))
	{
		Material.bPlanarMirror = Spec.value("planarMirror", Material.bPlanarMirror);
	}
	ApplyMaterialMaps(Resources, Material, Spec);
}

bool LoadMaterialFile(FResourceCache& Resources, const std::string& Path, FMaterial& Out)
{
	if (!IsLeonMaterialPath(Path))
	{
		std::cerr << "MaterialAsset: expected .lmat, got '" << Path << "'\n";
		return false;
	}
	return LoadLeonMaterialFile(Resources, Path, Out);
}

FMaterial MakeDefaultCheckerMaterial(FResourceCache& Resources)
{
	FMaterial Material;
	Material.Shading = EMaterialShadingModel::BlinnPhong;
	Material.Albedo = {1.0f, 1.0f, 1.0f};
	Material.Specular = {0.04f, 0.04f, 0.04f};
	Material.Metallic = 0.0f;
	Material.Shininess = 8.0f;
	Material.SyncRoughnessFromShininess();
	Material.bCastsShadows = true;
	Material.bPlanarMirror = false;
	Material.AlbedoMap = Resources.CheckerTexture(64);
	return Material;
}
