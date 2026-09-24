#pragma once

#include <glm/vec3.hpp>

#include <vector>


class UCameraComponent;

/// Shared party-fighter / arena framing: fixed yaw/pitch orbit that pulls back with separation.
struct ENGINE_API FArenaCameraParams {
    float FixedYawDegrees = 25.f;
    float FixedPitchDegrees = 45.f;
    float MinDistance = 10.f;
    float MaxDistance = 34.f;
    float DistanceBase = 9.f;
    float DistancePerSeparation = 1.55f;
    float TargetHeightOffset = 0.85f;
    /// Exponential lag toward desired target/distance (`1 - exp(-lagSpeed * dt)`).
    float LagSpeed = 8.f;
};

struct ENGINE_API FArenaCameraState {
    glm::vec3 Target{0};
    float Distance = 16.f;
};

/// Update from living pawn feet positions; empty list lerps toward floor-centered fallback.
void UpdateArenaCamera(UCameraComponent& Camera, FArenaCameraState& State, const FArenaCameraParams& Params,
                       const std::vector<glm::vec3>& LivingFeet, float DeltaTime,
                       float FloorYFallback);

