#pragma once

#include <glm/vec3.hpp>

#include "Camera/Camera.h"

namespace leon {

/// Keyboard move axes on the ground plane: x = strafe, z = forward (from mapped Move* actions).
struct MoveAxes2D {
    float x = 0.0f;
    float z = 0.0f;

    [[nodiscard]] bool any() const { return x != 0.0f || z != 0.0f; }
};

/// Project camera yaw onto XZ: +Z axis of the result is "forward" for `axes.z`.
[[nodiscard]] glm::vec3 cameraRelativeMoveXZ(const Camera& camera, const MoveAxes2D& axes);

/// Same as `cameraRelativeMoveXZ` but from an explicit yaw (e.g. desired SpringArm boom).
[[nodiscard]] glm::vec3 yawRelativeMoveXZ(float yawDegrees, const MoveAxes2D& axes);

} // namespace leon
