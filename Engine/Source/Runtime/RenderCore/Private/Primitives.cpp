#include <algorithm>
#include <cmath>
#include <cstdint>
#include "Primitives.h"
#include <numbers>


FMeshData MakeCube() {
    // 6 faces × 4 verts (unique normals/UVs per face corner).
    FMeshData Data;
    Data.Vertices = {
        // +Z
        {.Position = {-0.5f, -0.5f, 0.5f}, .Normal = {0, 0, 1}, .TexCoord = {0, 0}},
        {.Position = {0.5f, -0.5f, 0.5f}, .Normal = {0, 0, 1}, .TexCoord = {1, 0}},
        {.Position = {0.5f, 0.5f, 0.5f}, .Normal = {0, 0, 1}, .TexCoord = {1, 1}},
        {.Position = {-0.5f, 0.5f, 0.5f}, .Normal = {0, 0, 1}, .TexCoord = {0, 1}},
        // -Z
        {.Position = {0.5f, -0.5f, -0.5f}, .Normal = {0, 0, -1}, .TexCoord = {0, 0}},
        {.Position = {-0.5f, -0.5f, -0.5f}, .Normal = {0, 0, -1}, .TexCoord = {1, 0}},
        {.Position = {-0.5f, 0.5f, -0.5f}, .Normal = {0, 0, -1}, .TexCoord = {1, 1}},
        {.Position = {0.5f, 0.5f, -0.5f}, .Normal = {0, 0, -1}, .TexCoord = {0, 1}},
        // +Y
        {.Position = {-0.5f, 0.5f, 0.5f}, .Normal = {0, 1, 0}, .TexCoord = {0, 0}},
        {.Position = {0.5f, 0.5f, 0.5f}, .Normal = {0, 1, 0}, .TexCoord = {1, 0}},
        {.Position = {0.5f, 0.5f, -0.5f}, .Normal = {0, 1, 0}, .TexCoord = {1, 1}},
        {.Position = {-0.5f, 0.5f, -0.5f}, .Normal = {0, 1, 0}, .TexCoord = {0, 1}},
        // -Y
        {.Position = {-0.5f, -0.5f, -0.5f}, .Normal = {0, -1, 0}, .TexCoord = {0, 0}},
        {.Position = {0.5f, -0.5f, -0.5f}, .Normal = {0, -1, 0}, .TexCoord = {1, 0}},
        {.Position = {0.5f, -0.5f, 0.5f}, .Normal = {0, -1, 0}, .TexCoord = {1, 1}},
        {.Position = {-0.5f, -0.5f, 0.5f}, .Normal = {0, -1, 0}, .TexCoord = {0, 1}},
        // +X
        {.Position = {0.5f, -0.5f, 0.5f}, .Normal = {1, 0, 0}, .TexCoord = {0, 0}},
        {.Position = {0.5f, -0.5f, -0.5f}, .Normal = {1, 0, 0}, .TexCoord = {1, 0}},
        {.Position = {0.5f, 0.5f, -0.5f}, .Normal = {1, 0, 0}, .TexCoord = {1, 1}},
        {.Position = {0.5f, 0.5f, 0.5f}, .Normal = {1, 0, 0}, .TexCoord = {0, 1}},
        // -X
        {.Position = {-0.5f, -0.5f, -0.5f}, .Normal = {-1, 0, 0}, .TexCoord = {0, 0}},
        {.Position = {-0.5f, -0.5f, 0.5f}, .Normal = {-1, 0, 0}, .TexCoord = {1, 0}},
        {.Position = {-0.5f, 0.5f, 0.5f}, .Normal = {-1, 0, 0}, .TexCoord = {1, 1}},
        {.Position = {-0.5f, 0.5f, -0.5f}, .Normal = {-1, 0, 0}, .TexCoord = {0, 1}},
    };

    Data.Indices.reserve(36);
    for (std::uint32_t Face = 0; Face < 6; ++Face) {
        const std::uint32_t B = Face * 4;
        Data.Indices.insert(Data.Indices.end(), {B + 0, B + 1, B + 2, B + 0, B + 2, B + 3});
    }
    return Data;
}

FMeshData MakePlane(float Size, float UvScale) {
    const float H = Size * 0.5f;
    FMeshData Data;
    Data.Vertices = {
        {.Position = {-H, 0.0f, -H}, .Normal = {0.0f, 1.0f, 0.0f}, .TexCoord = {0.0f, 0.0f}},
        {.Position = {H, 0.0f, -H}, .Normal = {0.0f, 1.0f, 0.0f}, .TexCoord = {UvScale, 0.0f}},
        {.Position = {H, 0.0f, H}, .Normal = {0.0f, 1.0f, 0.0f}, .TexCoord = {UvScale, UvScale}},
        {.Position = {-H, 0.0f, H}, .Normal = {0.0f, 1.0f, 0.0f}, .TexCoord = {0.0f, UvScale}},
    };
    Data.Indices = {0, 2, 1, 0, 3, 2};
    return Data;
}

FMeshData MakeSphere(int Segments, int Rings) {
    Segments = std::max(Segments, 3);
    Rings = std::max(Rings, 2);

    FMeshData Data;
    Data.Vertices.reserve(static_cast<std::size_t>(Rings + 1) *
                          static_cast<std::size_t>(Segments + 1));
    Data.Indices.reserve(static_cast<std::size_t>(Rings) * static_cast<std::size_t>(Segments) * 6u);

    constexpr float Radius = 0.5f;
    for (int Y = 0; Y <= Rings; ++Y) {
        const auto V = static_cast<float>(Y) / static_cast<float>(Rings);
        const float Phi = V * std::numbers::pi_v<float>;
        const float SinPhi = std::sin(Phi);
        const float CosPhi = std::cos(Phi);
        for (int X = 0; X <= Segments; ++X) {
            const auto U = static_cast<float>(X) / static_cast<float>(Segments);
            const float Theta = U * 2.0f * std::numbers::pi_v<float>;
            const glm::vec3 Normal{std::cos(Theta) * SinPhi, CosPhi, std::sin(Theta) * SinPhi};
            Data.Vertices.push_back(
                FVertex{.Position = Normal * Radius, .Normal = Normal, .TexCoord = {U, 1.0f - V}});
        }
    }

    for (int Y = 0; Y < Rings; ++Y) {
        for (int X = 0; X < Segments; ++X) {
            const auto I0 = static_cast<std::uint32_t>(
                (static_cast<std::size_t>(Y) * static_cast<std::size_t>(Segments + 1)) +
                static_cast<std::size_t>(X));
            const auto I1 = I0 + static_cast<std::uint32_t>(Segments + 1);
            // CCW when viewed from outside (matches outward normals + back-face cull).
            Data.Indices.insert(Data.Indices.end(), {I0, I0 + 1, I1, I0 + 1, I1 + 1, I1});
        }
    }
    return Data;
}

