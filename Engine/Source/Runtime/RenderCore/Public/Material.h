#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <memory>


class Texture;

enum class EShadingModel {
    BlinnPhong,
    Unlit,
};

/// Roughness from Blinn shininess (high Ns → sharp env reflections).
[[nodiscard]] inline float roughnessFromShininess(float shininess) {
    const float s = std::max(shininess, 1.0f);
    return std::clamp(std::sqrt(2.0f / (s + 2.0f)), 0.04f, 1.0f);
}

/// Per-object surface for the forward lit pass.
/// specular/metallic + roughness drive Blinn highlights and HDR cubemap LOD.
/// uvScale tiles albedo/normal maps (Unreal-like Material Instance tiling).
struct Material {
    EShadingModel shading = EShadingModel::BlinnPhong;
    glm::vec3 albedo{0.55f, 0.72f, 0.85f};
    glm::vec3 specular{0.04f, 0.04f, 0.04f}; // F0 / MTL Ks (dielectric default ~4%)
    float metallic = 0.0f; // 0 = dielectric, 1 = metal (tints specular, kills diffuse)
    float alpha = 1.0f;    // < 1 → transparent queue (back-to-front)
    float shininess = 32.0f;
    float roughness = roughnessFromShininess(32.0f); // 0 = mirror, 1 = fully blurred env
    glm::vec2 uvScale{1.0f, 1.0f};                   // multiplies mesh UVs when sampling maps
    bool castsShadows = true;
    bool planarMirror = false;          // horizontal ground mirror (scene planar reflection pass)
    std::shared_ptr<Texture> albedoMap; // optional; white if null
    std::shared_ptr<Texture> normalMap; // optional; flat (+Z) if null

    [[nodiscard]] bool isTransparent() const { return alpha < 0.999f; }

    void syncRoughnessFromShininess() { roughness = roughnessFromShininess(shininess); }
};

