#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cmath>
#include "MeshData.h"
#include <vector>


void ComputeTangents(FMeshData& Data) {
    if (Data.empty()) {
        return;
    }

    std::vector<glm::vec3> TanAcc(Data.Vertices.size(), glm::vec3(0.0f));
    std::vector<glm::vec3> BitAcc(Data.Vertices.size(), glm::vec3(0.0f));

    for (std::size_t I = 0; I + 2 < Data.Indices.size(); I += 3) {
        const auto I0 = Data.Indices[I + 0];
        const auto I1 = Data.Indices[I + 1];
        const auto I2 = Data.Indices[I + 2];

        const FVertex& V0 = Data.Vertices[I0];
        const FVertex& V1 = Data.Vertices[I1];
        const FVertex& V2 = Data.Vertices[I2];

        const glm::vec3 E1 = V1.Position - V0.Position;
        const glm::vec3 E2 = V2.Position - V0.Position;
        const glm::vec2 D1 = V1.TexCoord - V0.TexCoord;
        const glm::vec2 D2 = V2.TexCoord - V0.TexCoord;

        const float Det = (D1.x * D2.y) - (D2.x * D1.y);
        if (std::abs(Det) < 1e-8f) {
            continue;
        }
        const float Inv = 1.0f / Det;
        const glm::vec3 Tangent = ((E1 * D2.y) - (E2 * D1.y)) * Inv;
        const glm::vec3 Bitangent = ((E2 * D1.x) - (E1 * D2.x)) * Inv;
        TanAcc[I0] += Tangent;
        TanAcc[I1] += Tangent;
        TanAcc[I2] += Tangent;
        BitAcc[I0] += Bitangent;
        BitAcc[I1] += Bitangent;
        BitAcc[I2] += Bitangent;
    }

    for (std::size_t I = 0; I < Data.Vertices.size(); ++I) {
        FVertex& Vertex = Data.Vertices[I];
        const glm::vec3 N = Vertex.Normal;
        glm::vec3 T = TanAcc[I];
        if (glm::dot(T, T) < 1e-8f) {
            T = std::abs(N.y) < 0.9f ? glm::normalize(glm::cross(N, {0, 1, 0}))
                                     : glm::normalize(glm::cross(N, {1, 0, 0}));
            Vertex.Tangent = glm::vec4(T, 1.0f);
            continue;
        }
        T = glm::normalize(T - (N * glm::dot(N, T)));
        const float Handedness = (glm::dot(glm::cross(N, T), BitAcc[I]) < 0.0f) ? -1.0f : 1.0f;
        Vertex.Tangent = glm::vec4(T, Handedness);
    }
}

