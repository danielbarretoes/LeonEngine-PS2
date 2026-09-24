#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


/// Interleaved GPU vertex attributes (matches Mesh VAO layout).
struct FVertex {
    glm::vec3 Position{};
    glm::vec3 Normal{};
    glm::vec2 TexCoord{};
    /// xyz = tangent; w = bitangent handedness (±1) for mirrored UVs.
    glm::vec4 Tangent{0.0f, 0.0f, 0.0f, 1.0f};
};

