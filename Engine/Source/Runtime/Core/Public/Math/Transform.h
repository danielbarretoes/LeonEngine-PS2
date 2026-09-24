#pragma once

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace leon {

/// TRS transform used to build model / normal matrices (Unreal-like FTransform lite).
struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotationDegrees{0.0f}; // XYZ Euler, degrees
    glm::vec3 scale{1.0f};

    [[nodiscard]] glm::mat4 modelMatrix() const;
    [[nodiscard]] glm::mat3 normalMatrix() const;
};

} // namespace leon
