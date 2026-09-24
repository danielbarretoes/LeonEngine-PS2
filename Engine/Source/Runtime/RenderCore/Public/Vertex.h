#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


/// Interleaved GPU vertex attributes (matches Mesh VAO layout).
struct Vertex {
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 texCoord{};
    /// xyz = tangent; w = bitangent handedness (±1) for mirrored UVs.
    glm::vec4 tangent{0.0f, 0.0f, 0.0f, 1.0f};
};

