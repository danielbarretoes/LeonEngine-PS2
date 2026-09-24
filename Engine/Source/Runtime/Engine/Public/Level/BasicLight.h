#pragma once

#include "Level/Light.h"
#include <string_view>


class Level;

/// Engine light primitives (Unreal-like DirectionalLight / PointLight).
enum class EBasicLight {
    Directional,
    Point,
};

/// Placeable light: FTransform + Unreal Details fields (intensity, lightColor, castShadows, …).
struct BasicLight {
    EBasicLight type = EBasicLight::Directional;
    FTransform transform{};
    glm::vec3 lightColor{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    bool castShadows = true;
    /// Directional: soft-shadow angular diameter (degrees). Ignored for Point today.
    float sourceAngle = kDefaultLightSourceAngleDegrees;
    /// Point: attenuation radius.
    float range = 8.0f;

    [[nodiscard]] static BasicLight directional(glm::vec3 rotationDegrees = {60.3f, 142.1f, 0.0f},
                                                glm::vec3 lightColor = {1.0f, 1.0f, 1.0f},
                                                float intensity = 1.0f);
    [[nodiscard]] static BasicLight point(glm::vec3 position = {0.0f, 2.0f, 0.0f},
                                          glm::vec3 lightColor = {1.0f, 1.0f, 1.0f},
                                          float intensity = 1.0f, float range = 8.0f);

    [[nodiscard]] DirectionalLight asDirectional() const;
    [[nodiscard]] PointLight asPoint() const;

    /// Append this light to the Level's directional or point list.
    void addTo(Level& level) const;
};

[[nodiscard]] bool tryParseBasicLightName(std::string_view name, EBasicLight& out);

