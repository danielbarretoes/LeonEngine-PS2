#pragma once

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "Math/Transform.h"


constexpr int kMaxDirectionalLights = 2;
constexpr int kMaxPointLights = 4;

/// Unreal DirectionalLight Source Angle default (~sun disc), in degrees.
constexpr float kDefaultLightSourceAngleDegrees = 0.5357f;

/// Light travel direction from Unreal-like pitch (X) / yaw (Y) degrees. Roll ignored.
[[nodiscard]] inline glm::vec3 lightDirectionFromRotation(const glm::vec3& rotationDegrees) {
    constexpr float kDegToRad = 0.017453292519943295769f;
    const float pitch = rotationDegrees.x * kDegToRad;
    const float yaw = rotationDegrees.y * kDegToRad;
    const float cp = std::cos(pitch);
    const glm::vec3 dir{std::sin(yaw) * cp, -std::sin(pitch), std::cos(yaw) * cp};
    const float len = glm::length(dir);
    return len > 1.0e-8f ? (dir / len) : glm::vec3{0.0f, -1.0f, 0.0f};
}

/// Inverse of `lightDirectionFromRotation` (roll = 0).
[[nodiscard]] inline glm::vec3 rotationFromLightDirection(const glm::vec3& direction) {
    constexpr float kRadToDeg = 57.295779513082320877f;
    const float len = glm::length(direction);
    const glm::vec3 d = len > 1.0e-8f ? (direction / len) : glm::vec3{0.0f, -1.0f, 0.0f};
    const float pitch = std::asin(std::clamp(-d.y, -1.0f, 1.0f));
    const float yaw = std::atan2(d.x, d.z);
    return {pitch * kRadToDeg, yaw * kRadToDeg, 0.0f};
}

/// Unreal-like DirectionalLight: transform drives aim; no raw direction field.
struct DirectionalLight {
    FTransform transform{{0.0f, 0.0f, 0.0f}, {60.3f, 142.1f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    glm::vec3 lightColor{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    bool castShadows = true;
    float sourceAngle = kDefaultLightSourceAngleDegrees;
    /// Session-stable editor selection id (0 = unassigned). Not serialized.
    std::uint64_t editorId = 0;

    [[nodiscard]] glm::vec3 GetDirection() const {
        return lightDirectionFromRotation(transform.RotationDegrees);
    }
};

/// Unreal-like PointLight: location from transform; attenuation `range`.
struct PointLight {
    FTransform transform{{0.0f, 2.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    glm::vec3 lightColor{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 8.0f;
    bool castShadows = false;
    /// Session-stable editor selection id (0 = unassigned). Not serialized.
    std::uint64_t editorId = 0;

    /// Optional orbit animation (Level JSON `orbit`); preserved for save round-trip.
    bool hasOrbit = false;
    float orbitRadius = 1.0f;
    float orbitHeight = 1.0f;
    float orbitHeightAmp = 0.0f;
    float orbitSpeed = 1.0f;
};

