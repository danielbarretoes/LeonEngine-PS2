#pragma once

#include "Level/Light.h"
#include <string_view>


class ULevel;

/// Engine light primitives (Unreal-like FDirectionalLight / FPointLight).
enum class EBasicLight {
    Directional,
    Point,
};

/// Placeable light: FTransform + Unreal Details fields (intensity, lightColor, castShadows, …).
struct FBasicLight {
    EBasicLight Type = EBasicLight::Directional;
    FTransform Transform{};
    glm::vec3 LightColor{1.0f, 1.0f, 1.0f};
    float Intensity = 1.0f;
    bool bCastShadows = true;
    /// Directional: soft-shadow angular diameter (degrees). Ignored for Point today.
    float SourceAngle = DefaultLightSourceAngleDegrees;
    /// Point: attenuation radius.
    float Range = 8.0f;

    [[nodiscard]] static FBasicLight Directional(glm::vec3 RotationDegrees = {60.3f, 142.1f, 0.0f},
                                                glm::vec3 InLightColor = {1.0f, 1.0f, 1.0f},
                                                float InIntensity = 1.0f);
    [[nodiscard]] static FBasicLight Point(glm::vec3 Position = {0.0f, 2.0f, 0.0f},
                                          glm::vec3 InLightColor = {1.0f, 1.0f, 1.0f},
                                          float InIntensity = 1.0f, float InRange = 8.0f);

    [[nodiscard]] FDirectionalLight AsDirectional() const;
    [[nodiscard]] FPointLight AsPoint() const;

    /// Append this light to the Level's directional or point list.
    void AddTo(ULevel& Level) const;
};

[[nodiscard]] bool TryParseBasicLightName(std::string_view Name, EBasicLight& Out);

