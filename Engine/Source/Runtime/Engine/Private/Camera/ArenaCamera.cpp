#include "Camera/ArenaCamera.h"

#include <glm/common.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include "Camera/Camera.h"

namespace {

[[nodiscard]] float DistXZ(const glm::vec3& a, const glm::vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

[[nodiscard]] float ExpAlpha(float speed, float dt) {
    return (speed <= 0.0f || dt <= 0.0f) ? 1.0f : (1.0f - std::exp(-speed * dt));
}

} // namespace

void UpdateArenaCamera(Camera& camera, ArenaCameraState& state, const ArenaCameraParams& params,
                       const std::vector<glm::vec3>& livingFeet, float deltaTime,
                       float floorYFallback) {
    glm::vec3 desiredTarget = state.target;
    float desiredDistance = state.distance;

    if (!livingFeet.empty()) {
        glm::vec3 sum{0.0f};
        for (const glm::vec3& feet : livingFeet) {
            sum += feet + glm::vec3{0.0f, params.targetHeightOffset, 0.0f};
        }
        desiredTarget = sum / static_cast<float>(livingFeet.size());

        float maxSep = 0.0f;
        for (const glm::vec3& feet : livingFeet) {
            const glm::vec3 focus = feet + glm::vec3{0.0f, params.targetHeightOffset, 0.0f};
            maxSep = (std::max)(maxSep, DistXZ(focus, desiredTarget));
        }
        desiredDistance =
            std::clamp(params.distanceBase + maxSep * params.distancePerSeparation,
                       params.minDistance, params.maxDistance);
    } else {
        // No living pawns: frame arena center at standing height; distance ≈ prior Furytoon empty
        // framing (distanceBase * 2 → 18 with defaults).
        desiredTarget = {0.0f, floorYFallback + params.targetHeightOffset, 0.0f};
        desiredDistance = std::clamp(params.distanceBase * 2.0f, params.minDistance, params.maxDistance);
    }

    const float a = ExpAlpha(params.lagSpeed, deltaTime);
    state.target = glm::mix(state.target, desiredTarget, a);
    state.distance = glm::mix(state.distance, desiredDistance, a);

    camera.SetMode(ECameraMode::Orbit);
    camera.SetTarget(state.target);
    camera.SetDistance(state.distance);
    camera.SetYawPitch(params.fixedYawDegrees, params.fixedPitchDegrees);
}

