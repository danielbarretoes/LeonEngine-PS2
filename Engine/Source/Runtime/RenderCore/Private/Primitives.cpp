#include <algorithm>
#include <cmath>
#include <cstdint>
#include "Primitives.h"
#include <numbers>

namespace leon {

MeshData MakeCube() {
    // 6 faces × 4 verts (unique normals/UVs per face corner).
    MeshData data;
    data.vertices = {
        // +Z
        {.position = {-0.5f, -0.5f, 0.5f}, .normal = {0, 0, 1}, .texCoord = {0, 0}},
        {.position = {0.5f, -0.5f, 0.5f}, .normal = {0, 0, 1}, .texCoord = {1, 0}},
        {.position = {0.5f, 0.5f, 0.5f}, .normal = {0, 0, 1}, .texCoord = {1, 1}},
        {.position = {-0.5f, 0.5f, 0.5f}, .normal = {0, 0, 1}, .texCoord = {0, 1}},
        // -Z
        {.position = {0.5f, -0.5f, -0.5f}, .normal = {0, 0, -1}, .texCoord = {0, 0}},
        {.position = {-0.5f, -0.5f, -0.5f}, .normal = {0, 0, -1}, .texCoord = {1, 0}},
        {.position = {-0.5f, 0.5f, -0.5f}, .normal = {0, 0, -1}, .texCoord = {1, 1}},
        {.position = {0.5f, 0.5f, -0.5f}, .normal = {0, 0, -1}, .texCoord = {0, 1}},
        // +Y
        {.position = {-0.5f, 0.5f, 0.5f}, .normal = {0, 1, 0}, .texCoord = {0, 0}},
        {.position = {0.5f, 0.5f, 0.5f}, .normal = {0, 1, 0}, .texCoord = {1, 0}},
        {.position = {0.5f, 0.5f, -0.5f}, .normal = {0, 1, 0}, .texCoord = {1, 1}},
        {.position = {-0.5f, 0.5f, -0.5f}, .normal = {0, 1, 0}, .texCoord = {0, 1}},
        // -Y
        {.position = {-0.5f, -0.5f, -0.5f}, .normal = {0, -1, 0}, .texCoord = {0, 0}},
        {.position = {0.5f, -0.5f, -0.5f}, .normal = {0, -1, 0}, .texCoord = {1, 0}},
        {.position = {0.5f, -0.5f, 0.5f}, .normal = {0, -1, 0}, .texCoord = {1, 1}},
        {.position = {-0.5f, -0.5f, 0.5f}, .normal = {0, -1, 0}, .texCoord = {0, 1}},
        // +X
        {.position = {0.5f, -0.5f, 0.5f}, .normal = {1, 0, 0}, .texCoord = {0, 0}},
        {.position = {0.5f, -0.5f, -0.5f}, .normal = {1, 0, 0}, .texCoord = {1, 0}},
        {.position = {0.5f, 0.5f, -0.5f}, .normal = {1, 0, 0}, .texCoord = {1, 1}},
        {.position = {0.5f, 0.5f, 0.5f}, .normal = {1, 0, 0}, .texCoord = {0, 1}},
        // -X
        {.position = {-0.5f, -0.5f, -0.5f}, .normal = {-1, 0, 0}, .texCoord = {0, 0}},
        {.position = {-0.5f, -0.5f, 0.5f}, .normal = {-1, 0, 0}, .texCoord = {1, 0}},
        {.position = {-0.5f, 0.5f, 0.5f}, .normal = {-1, 0, 0}, .texCoord = {1, 1}},
        {.position = {-0.5f, 0.5f, -0.5f}, .normal = {-1, 0, 0}, .texCoord = {0, 1}},
    };

    data.indices.reserve(36);
    for (std::uint32_t face = 0; face < 6; ++face) {
        const std::uint32_t b = face * 4;
        data.indices.insert(data.indices.end(), {b + 0, b + 1, b + 2, b + 0, b + 2, b + 3});
    }
    return data;
}

MeshData MakePlane(float size, float uvScale) {
    const float h = size * 0.5f;
    MeshData data;
    data.vertices = {
        {.position = {-h, 0.0f, -h}, .normal = {0.0f, 1.0f, 0.0f}, .texCoord = {0.0f, 0.0f}},
        {.position = {h, 0.0f, -h}, .normal = {0.0f, 1.0f, 0.0f}, .texCoord = {uvScale, 0.0f}},
        {.position = {h, 0.0f, h}, .normal = {0.0f, 1.0f, 0.0f}, .texCoord = {uvScale, uvScale}},
        {.position = {-h, 0.0f, h}, .normal = {0.0f, 1.0f, 0.0f}, .texCoord = {0.0f, uvScale}},
    };
    data.indices = {0, 2, 1, 0, 3, 2};
    return data;
}

MeshData MakeSphere(int segments, int rings) {
    segments = std::max(segments, 3);
    rings = std::max(rings, 2);

    MeshData data;
    data.vertices.reserve(static_cast<std::size_t>(rings + 1) *
                          static_cast<std::size_t>(segments + 1));
    data.indices.reserve(static_cast<std::size_t>(rings) * static_cast<std::size_t>(segments) * 6u);

    constexpr float radius = 0.5f;
    for (int y = 0; y <= rings; ++y) {
        const auto v = static_cast<float>(y) / static_cast<float>(rings);
        const float phi = v * std::numbers::pi_v<float>;
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);
        for (int x = 0; x <= segments; ++x) {
            const auto u = static_cast<float>(x) / static_cast<float>(segments);
            const float theta = u * 2.0f * std::numbers::pi_v<float>;
            const glm::vec3 normal{std::cos(theta) * sinPhi, cosPhi, std::sin(theta) * sinPhi};
            data.vertices.push_back(
                Vertex{.position = normal * radius, .normal = normal, .texCoord = {u, 1.0f - v}});
        }
    }

    for (int y = 0; y < rings; ++y) {
        for (int x = 0; x < segments; ++x) {
            const auto i0 = static_cast<std::uint32_t>(
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(segments + 1)) +
                static_cast<std::size_t>(x));
            const auto i1 = i0 + static_cast<std::uint32_t>(segments + 1);
            // CCW when viewed from outside (matches outward normals + back-face cull).
            data.indices.insert(data.indices.end(), {i0, i0 + 1, i1, i0 + 1, i1 + 1, i1});
        }
    }
    return data;
}

} // namespace leon
