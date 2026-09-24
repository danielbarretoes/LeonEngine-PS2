#pragma once

#include <glm/vec3.hpp>

#include <vector>


class Camera;

/// Shared party-fighter / arena framing: fixed yaw/pitch orbit that pulls back with separation.
struct ArenaCameraParams {
    float fixedYawDegrees = 25.f;
    float fixedPitchDegrees = 45.f;
    float minDistance = 10.f;
    float maxDistance = 34.f;
    float distanceBase = 9.f;
    float distancePerSeparation = 1.55f;
    float targetHeightOffset = 0.85f;
    /// Exponential lag toward desired target/distance (`1 - exp(-lagSpeed * dt)`).
    float lagSpeed = 8.f;
};

struct ArenaCameraState {
    glm::vec3 target{0};
    float distance = 16.f;
};

/// Update from living pawn feet positions; empty list lerps toward floor-centered fallback.
void UpdateArenaCamera(Camera& camera, ArenaCameraState& state, const ArenaCameraParams& params,
                       const std::vector<glm::vec3>& livingFeet, float deltaTime,
                       float floorYFallback);

