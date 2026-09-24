#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>
#include "GameFramework/Input.h"

namespace {

constexpr float kDegToRad = glm::pi<float>() / 180.0f;

} // namespace

glm::vec3 yawRelativeMoveXZ(float yawDegrees, const MoveAxes2D& axes) {
    if (!axes.any()) {
        return glm::vec3{0.0f};
    }

    const float yawRad = yawDegrees * kDegToRad;
    // Match Camera orbit: world offset ≈ (cos(p)*cos(y), …, cos(p)*sin(y)); look ≈ -offset.xz
    const glm::vec3 forward{-std::cos(yawRad), 0.0f, -std::sin(yawRad)};
    const glm::vec3 right{std::sin(yawRad), 0.0f, -std::cos(yawRad)};

    glm::vec3 move = (forward * axes.z) + (right * axes.x);
    const float len = glm::length(move);
    if (len > 1.0e-4f) {
        move /= len;
    }
    return move;
}

glm::vec3 cameraRelativeMoveXZ(const Camera& camera, const MoveAxes2D& axes) {
    return yawRelativeMoveXZ(camera.YawDegrees(), axes);
}

