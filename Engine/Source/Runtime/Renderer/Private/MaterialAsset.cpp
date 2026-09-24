#include <algorithm>
#include <iostream>
#include "Misc/Paths.h"
#include "LeonMaterialFormat.h"
#include "MaterialAsset.h"
#include "ResourceCache.h"
#include <nlohmann/json.hpp>

namespace {

glm::vec3 readVec3(const nlohmann::json& j, const glm::vec3& fallback) {
    if (!j.is_array() || j.size() < 3) {
        return fallback;
    }
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

void applyMaterialMaps(ResourceCache& resources, Material& material, const nlohmann::json& object) {
    if (object.contains("albedoMap") && object["albedoMap"].is_string()) {
        const std::string key = object["albedoMap"].get<std::string>();
        if (key == "checker") {
            material.albedoMap = resources.CheckerTexture(64);
        } else {
            material.albedoMap = resources.LoadTexture(FPaths::ResolveAssetPath(key));
        }
    }
    if (object.contains("normalMap") && object["normalMap"].is_string()) {
        const std::string key = object["normalMap"].get<std::string>();
        if (key == "bump") {
            material.normalMap = resources.BumpNormalTexture(256);
        } else {
            material.normalMap = resources.LoadTexture(FPaths::ResolveAssetPath(key));
        }
    }
}

} // namespace

bool HasMaterialSurfaceFields(const nlohmann::json& spec) {
    return spec.contains("albedo") || spec.contains("alpha") || spec.contains("specular") ||
           spec.contains("metallic") || spec.contains("shininess") || spec.contains("roughness") ||
           spec.contains("unlit") || spec.contains("albedoMap") || spec.contains("normalMap") ||
           spec.contains("uvScale") || spec.contains("tiling");
}

void PatchMaterialFromJson(ResourceCache& resources, Material& material,
                           const nlohmann::json& spec) {
    if (spec.contains("unlit") && spec["unlit"].is_boolean() && spec["unlit"].get<bool>()) {
        material.shading = EShadingModel::Unlit;
    }
    if (spec.contains("albedo")) {
        material.albedo = readVec3(spec["albedo"], material.albedo);
    }
    if (spec.contains("specular")) {
        material.specular = readVec3(spec["specular"], material.specular);
    }
    if (spec.contains("metallic")) {
        material.metallic = spec.value("metallic", material.metallic);
    }
    if (spec.contains("alpha")) {
        material.alpha = spec.value("alpha", material.alpha);
    }
    if (spec.contains("shininess")) {
        material.shininess = spec.value("shininess", material.shininess);
        if (!spec.contains("roughness")) {
            material.syncRoughnessFromShininess();
        }
    }
    if (spec.contains("roughness")) {
        material.roughness = std::clamp(spec.value("roughness", material.roughness), 0.04f, 1.0f);
    }
    const nlohmann::json* uvNode = nullptr;
    if (spec.contains("uvScale")) {
        uvNode = &spec["uvScale"];
    } else if (spec.contains("tiling")) {
        uvNode = &spec["tiling"];
    }
    if (uvNode != nullptr) {
        if (uvNode->is_number()) {
            const float s = uvNode->get<float>();
            material.uvScale = {s, s};
        } else if (uvNode->is_array() && uvNode->size() >= 2) {
            material.uvScale = {(*uvNode)[0].get<float>(), (*uvNode)[1].get<float>()};
        }
    }
    if (spec.contains("castsShadows")) {
        material.castsShadows = spec.value("castsShadows", material.castsShadows);
    }
    if (spec.contains("planarMirror")) {
        material.planarMirror = spec.value("planarMirror", material.planarMirror);
    }
    applyMaterialMaps(resources, material, spec);
}

bool LoadMaterialFile(ResourceCache& resources, const std::string& path, Material& out) {
    if (!IsLeonMaterialPath(path)) {
        std::cerr << "MaterialAsset: expected .lmat, got '" << path << "'\n";
        return false;
    }
    return LoadLeonMaterialFile(resources, path, out);
}

Material MakeDefaultCheckerMaterial(ResourceCache& resources) {
    Material material;
    material.shading = EShadingModel::BlinnPhong;
    material.albedo = {1.0f, 1.0f, 1.0f};
    material.specular = {0.04f, 0.04f, 0.04f};
    material.metallic = 0.0f;
    material.shininess = 8.0f;
    material.syncRoughnessFromShininess();
    material.castsShadows = true;
    material.planarMirror = false;
    material.albedoMap = resources.CheckerTexture(64);
    return material;
}

