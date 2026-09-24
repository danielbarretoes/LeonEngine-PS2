#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <memory>


class UTexture2D;

enum class EMaterialShadingModel {
    BlinnPhong,
    Unlit,
};

/// Roughness from Blinn shininess (high Ns → sharp env reflections).
[[nodiscard]] inline float RoughnessFromShininess(float InShininess) {
    const float S = std::max(InShininess, 1.0f);
    return std::clamp(std::sqrt(2.0f / (S + 2.0f)), 0.04f, 1.0f);
}

/// Per-object surface for the forward lit pass.
/// specular/metallic + roughness drive Blinn highlights and HDR cubemap LOD.
/// uvScale tiles albedo/normal maps (Unreal-like FMaterial Instance tiling).
struct RENDERCORE_API FMaterial {
    EMaterialShadingModel Shading = EMaterialShadingModel::BlinnPhong;
    glm::vec3 Albedo{0.55f, 0.72f, 0.85f};
    glm::vec3 Specular{0.04f, 0.04f, 0.04f}; // F0 / MTL Ks (dielectric default ~4%)
    float Metallic = 0.0f; // 0 = dielectric, 1 = metal (tints specular, kills diffuse)
    float Alpha = 1.0f;    // < 1 → transparent queue (back-to-front)
    float Shininess = 32.0f;
    float Roughness = RoughnessFromShininess(32.0f); // 0 = mirror, 1 = fully blurred env
    glm::vec2 UvScale{1.0f, 1.0f};                   // multiplies mesh UVs when sampling maps
    bool bCastsShadows = true;
    bool bPlanarMirror = false;          // horizontal ground mirror (scene planar reflection pass)
    std::shared_ptr<UTexture2D> AlbedoMap; // optional; white if null
    std::shared_ptr<UTexture2D> NormalMap; // optional; flat (+Z) if null

    [[nodiscard]] bool IsTransparent() const { return Alpha < 0.999f; }

    void SyncRoughnessFromShininess() { Roughness = RoughnessFromShininess(Shininess); }
};

