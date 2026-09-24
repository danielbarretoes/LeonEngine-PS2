#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <tiny_obj_loader.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include "ObjImport.h"
#include <map>
#include <unordered_map>
#include <vector>

namespace {

class VertexKey {
public:
    VertexKey(int positionIndex, int normalIndex, int texcoordIndex)
        : positionIndex_(positionIndex), normalIndex_(normalIndex), texcoordIndex_(texcoordIndex) {}

    [[nodiscard]] bool operator==(const VertexKey& other) const {
        return positionIndex_ == other.positionIndex_ && normalIndex_ == other.normalIndex_ &&
               texcoordIndex_ == other.texcoordIndex_;
    }

    [[nodiscard]] int positionIndex() const { return positionIndex_; }
    [[nodiscard]] int normalIndex() const { return normalIndex_; }
    [[nodiscard]] int texcoordIndex() const { return texcoordIndex_; }

private:
    int positionIndex_ = 0;
    int normalIndex_ = 0;
    int texcoordIndex_ = 0;
};

struct VertexKeyHash {
    std::size_t operator()(const VertexKey& key) const noexcept {
        auto h = static_cast<std::size_t>(key.positionIndex());
        h ^= static_cast<std::size_t>(key.normalIndex()) + 0x9e3779b97f4a7c15ULL + (h << 6) +
             (h >> 2);
        h ^= static_cast<std::size_t>(key.texcoordIndex()) + 0x9e3779b97f4a7c15ULL + (h << 6) +
             (h >> 2);
        return h;
    }
};

void computeSmoothNormals(FMeshData& data) {
    for (auto& vertex : data.vertices) {
        vertex.normal = {0.0f, 0.0f, 0.0f};
    }

    for (std::size_t i = 0; i + 2 < data.indices.size(); i += 3) {
        const auto i0 = data.indices[i + 0];
        const auto i1 = data.indices[i + 1];
        const auto i2 = data.indices[i + 2];

        const glm::vec3 edge1 = data.vertices[i1].position - data.vertices[i0].position;
        const glm::vec3 edge2 = data.vertices[i2].position - data.vertices[i0].position;
        const glm::vec3 faceNormal = glm::cross(edge1, edge2);
        if (glm::dot(faceNormal, faceNormal) < 1e-20f) {
            continue;
        }

        const glm::vec3 n = glm::normalize(faceNormal);
        data.vertices[i0].normal += n;
        data.vertices[i1].normal += n;
        data.vertices[i2].normal += n;
    }

    for (auto& vertex : data.vertices) {
        if (glm::dot(vertex.normal, vertex.normal) > 0.0f) {
            vertex.normal = glm::normalize(vertex.normal);
        } else {
            vertex.normal = {0.0f, 1.0f, 0.0f};
        }
    }
}

bool indexInRange(int index, std::size_t count, int components) {
    if (index < 0) {
        return false;
    }
    const auto needed = static_cast<std::size_t>(index + 1) * static_cast<std::size_t>(components);
    return needed <= count;
}

bool faceIndicesValid(const tinyobj::attrib_t& attrib, const tinyobj::index_t& index,
                      bool hasFileNormals, bool hasTexcoords) {
    if (!indexInRange(index.vertex_index, attrib.vertices.size(), 3)) {
        return false;
    }
    if (hasFileNormals && index.normal_index >= 0 &&
        !indexInRange(index.normal_index, attrib.normals.size(), 3)) {
        return false;
    }
    if (hasTexcoords && index.texcoord_index >= 0 &&
        !indexInRange(index.texcoord_index, attrib.texcoords.size(), 2)) {
        return false;
    }
    return true;
}

std::uint32_t getOrCreateVertex(FMeshData& data,
                                std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash>& unique,
                                const tinyobj::attrib_t& attrib, const tinyobj::index_t& index,
                                bool hasFileNormals, bool hasTexcoords) {
    const VertexKey key{index.vertex_index, index.normal_index, index.texcoord_index};
    if (const auto found = unique.find(key); found != unique.end()) {
        return found->second;
    }

    FVertex vertex{};
    const auto vi = static_cast<std::size_t>(index.vertex_index) * 3u;
    vertex.position = {
        attrib.vertices[vi + 0],
        attrib.vertices[vi + 1],
        attrib.vertices[vi + 2],
    };

    if (hasFileNormals && index.normal_index >= 0) {
        const auto ni = static_cast<std::size_t>(index.normal_index) * 3u;
        const glm::vec3 n{
            attrib.normals[ni + 0],
            attrib.normals[ni + 1],
            attrib.normals[ni + 2],
        };
        vertex.normal = (glm::dot(n, n) > 0.0f) ? glm::normalize(n) : glm::vec3{0, 1, 0};
    } else {
        vertex.normal = {0.0f, 1.0f, 0.0f};
    }

    if (hasTexcoords && index.texcoord_index >= 0) {
        const auto ti = static_cast<std::size_t>(index.texcoord_index) * 2u;
        vertex.texCoord = {
            attrib.texcoords[ti + 0],
            attrib.texcoords[ti + 1],
        };
    }

    const auto newIndex = static_cast<std::uint32_t>(data.vertices.size());
    unique.emplace(key, newIndex);
    data.vertices.push_back(vertex);
    return newIndex;
}

FMaterial materialFromTiny(const tinyobj::material_t& src) {
    FMaterial material;
    material.shading = EMaterialShadingModel::BlinnPhong;
    material.albedo = {src.diffuse[0], src.diffuse[1], src.diffuse[2]};
    material.specular = {src.specular[0], src.specular[1], src.specular[2]};
    material.alpha = src.dissolve;
    // Max/OBJ often exports low Ns; remap so highlights read clearly in Blinn-Phong.
    const float ns = std::max(src.shininess, 1.0f);
    material.shininess = std::clamp((ns * ns * 0.25f) + (ns * 2.0f), 8.0f, 256.0f);

    // Heuristic metalness from MTL (no explicit metal map): strong Ks relative to Kd.
    const float kd = (material.albedo.x + material.albedo.y + material.albedo.z) / 3.0f;
    const float ks = (material.specular.x + material.specular.y + material.specular.z) / 3.0f;
    if (ks > 0.2f) {
        material.metallic = std::clamp((ks - 0.15f) / 0.6f, 0.0f, 1.0f);
        // Painted metals in this asset use gray Ks; keep some metal even when Kd is dark.
        if (kd < 0.35f && ks >= 0.35f) {
            material.metallic = std::max(material.metallic, 0.65f);
        }
    }
    // Ensure specular floor so dielectrics still catch highlights.
    if (ks < 0.04f) {
        material.specular = {0.04f, 0.04f, 0.04f};
    }

    material.syncRoughnessFromShininess();
    material.castsShadows = material.alpha >= 0.999f;
    return material;
}

} // namespace

FMeshData LoadObj(const std::string& path) {
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;
    config.mtl_search_path = std::filesystem::path(path).parent_path().string();

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, config)) {
        if (!reader.Error().empty()) {
            std::cerr << "tinyobjloader: " << reader.Error() << '\n';
        }
        return {};
    }

    if (!reader.Warning().empty()) {
        std::cerr << "tinyobjloader: " << reader.Warning() << '\n';
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& tinyMaterials = reader.GetMaterials();

    std::size_t indexEstimate = 0;
    for (const auto& shape : shapes) {
        indexEstimate += shape.mesh.indices.size();
    }

    FMeshData data;
    data.vertices.reserve(indexEstimate);

    std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> unique;
    unique.reserve(indexEstimate);

    // materialId → triangle indices (grouped so each FMeshSection is contiguous).
    std::map<int, std::vector<std::uint32_t>> indicesByMaterial;

    const bool hasFileNormals = !attrib.normals.empty();
    const bool hasTexcoords = !attrib.texcoords.empty();

    for (const auto& shape : shapes) {
        std::size_t indexOffset = 0;
        for (std::size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face) {
            const unsigned int faceVerts = shape.mesh.num_face_vertices[face];
            const int materialId =
                face < shape.mesh.material_ids.size() ? shape.mesh.material_ids[face] : -1;

            // Triangulated OBJ: expect 3 verts per face.
            if (faceVerts != 3) {
                indexOffset += faceVerts;
                continue;
            }

            const tinyobj::index_t& i0 = shape.mesh.indices[indexOffset + 0];
            const tinyobj::index_t& i1 = shape.mesh.indices[indexOffset + 1];
            const tinyobj::index_t& i2 = shape.mesh.indices[indexOffset + 2];
            indexOffset += 3;

            if (!faceIndicesValid(attrib, i0, hasFileNormals, hasTexcoords) ||
                !faceIndicesValid(attrib, i1, hasFileNormals, hasTexcoords) ||
                !faceIndicesValid(attrib, i2, hasFileNormals, hasTexcoords)) {
                std::cerr << "MeshData: skipping face with out-of-range indices in " << path
                          << '\n';
                continue;
            }

            auto& bucket = indicesByMaterial[materialId];
            bucket.push_back(
                getOrCreateVertex(data, unique, attrib, i0, hasFileNormals, hasTexcoords));
            bucket.push_back(
                getOrCreateVertex(data, unique, attrib, i1, hasFileNormals, hasTexcoords));
            bucket.push_back(
                getOrCreateVertex(data, unique, attrib, i2, hasFileNormals, hasTexcoords));
        }
    }

    if (data.vertices.empty() || indicesByMaterial.empty()) {
        std::cerr << "Mesh has no geometry: " << path << '\n';
        return {};
    }

    if (!tinyMaterials.empty()) {
        data.materials.reserve(tinyMaterials.size());
        data.albedoMapPaths.reserve(tinyMaterials.size());
        const std::filesystem::path objDir = std::filesystem::path(path).parent_path();
        for (const tinyobj::material_t& src : tinyMaterials) {
            data.materials.push_back(materialFromTiny(src));
            if (!src.diffuse_texname.empty()) {
                data.albedoMapPaths.push_back((objDir / src.diffuse_texname).string());
            } else {
                data.albedoMapPaths.emplace_back();
            }
        }
    }

    data.indices.reserve(indexEstimate);
    for (auto& [materialId, bucket] : indicesByMaterial) {
        if (bucket.empty()) {
            continue;
        }

        int slot = 0;
        if (materialId >= 0 && materialId < static_cast<int>(data.materials.size())) {
            slot = materialId;
        } else if (!data.materials.empty()) {
            slot = 0;
        }

        FMeshSection sub;
        sub.indexOffset = static_cast<int>(data.indices.size());
        sub.indexCount = static_cast<int>(bucket.size());
        sub.materialIndex = slot;
        data.indices.insert(data.indices.end(), bucket.begin(), bucket.end());
        data.submeshes.push_back(sub);
    }

    if (data.materials.empty()) {
        data.materials.push_back(FMaterial{});
        data.albedoMapPaths.emplace_back();
        for (FMeshSection& sub : data.submeshes) {
            sub.materialIndex = 0;
        }
    }

    if (!hasFileNormals) {
        computeSmoothNormals(data);
    }

    std::cout << "OBJ '" << path << "': " << data.vertices.size() << " verts, "
              << (data.indices.size() / 3) << " tris, " << data.submeshes.size() << " submeshes, "
              << data.materials.size() << " materials\n";
    return data;
}

