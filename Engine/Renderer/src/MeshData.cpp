#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cmath>
#include <leon/render/MeshData.h>
#include <vector>

namespace leon {

void ComputeTangents(MeshData& data) {
    if (data.empty()) {
        return;
    }

    std::vector<glm::vec3> tanAcc(data.vertices.size(), glm::vec3(0.0f));
    std::vector<glm::vec3> bitAcc(data.vertices.size(), glm::vec3(0.0f));

    for (std::size_t i = 0; i + 2 < data.indices.size(); i += 3) {
        const auto i0 = data.indices[i + 0];
        const auto i1 = data.indices[i + 1];
        const auto i2 = data.indices[i + 2];

        const Vertex& v0 = data.vertices[i0];
        const Vertex& v1 = data.vertices[i1];
        const Vertex& v2 = data.vertices[i2];

        const glm::vec3 e1 = v1.position - v0.position;
        const glm::vec3 e2 = v2.position - v0.position;
        const glm::vec2 d1 = v1.texCoord - v0.texCoord;
        const glm::vec2 d2 = v2.texCoord - v0.texCoord;

        const float det = (d1.x * d2.y) - (d2.x * d1.y);
        if (std::abs(det) < 1e-8f) {
            continue;
        }
        const float inv = 1.0f / det;
        const glm::vec3 tangent = ((e1 * d2.y) - (e2 * d1.y)) * inv;
        const glm::vec3 bitangent = ((e2 * d1.x) - (e1 * d2.x)) * inv;
        tanAcc[i0] += tangent;
        tanAcc[i1] += tangent;
        tanAcc[i2] += tangent;
        bitAcc[i0] += bitangent;
        bitAcc[i1] += bitangent;
        bitAcc[i2] += bitangent;
    }

    for (std::size_t i = 0; i < data.vertices.size(); ++i) {
        Vertex& vertex = data.vertices[i];
        const glm::vec3 n = vertex.normal;
        glm::vec3 t = tanAcc[i];
        if (glm::dot(t, t) < 1e-8f) {
            t = std::abs(n.y) < 0.9f ? glm::normalize(glm::cross(n, {0, 1, 0}))
                                     : glm::normalize(glm::cross(n, {1, 0, 0}));
            vertex.tangent = glm::vec4(t, 1.0f);
            continue;
        }
        t = glm::normalize(t - (n * glm::dot(n, t)));
        const float handedness = (glm::dot(glm::cross(n, t), bitAcc[i]) < 0.0f) ? -1.0f : 1.0f;
        vertex.tangent = glm::vec4(t, handedness);
    }
}

} // namespace leon
