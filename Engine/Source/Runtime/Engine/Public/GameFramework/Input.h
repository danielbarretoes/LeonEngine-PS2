#pragma once

#include <glm/vec3.hpp>

#include "Camera/CameraComponent.h"


/// Keyboard move axes on the ground plane: x = strafe, z = forward (from mapped Move* actions).
struct FMoveAxes2D {
    float X = 0.0f;
    float Z = 0.0f;

    [[nodiscard]] bool Any() const { return X != 0.0f || Z != 0.0f; }
};

/// Project camera yaw onto XZ: +Z axis of the result is "forward" for `axes.z`.
[[nodiscard]] glm::vec3 CameraRelativeMoveXz(const UCameraComponent& Camera, const FMoveAxes2D& Axes);

/// Same as `cameraRelativeMoveXZ` but from an explicit yaw (e.g. desired SpringArm boom).
[[nodiscard]] glm::vec3 YawRelativeMoveXz(float YawDegrees, const FMoveAxes2D& Axes);

