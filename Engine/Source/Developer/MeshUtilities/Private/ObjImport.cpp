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

class FVertexKey {
public:
    FVertexKey(int positionIndex, int normalIndex, int texcoordIndex)
        : positionIndex_(positionIndex), normalIndex_(normalIndex), texcoordIndex_(texcoordIndex) {}

    [[nodiscard]] bool operator==(const FVertexKey& other) const {
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

struct FVertexKeyHash {
    std::size_t operator()(const FVertexKey& key) const noexcept {
        auto h = static_cast<std::size_t>(key.positionIndex());
        h ^= static_cast<std::size_t>(key.normalIndex()) + 0x9e3779b97f4a7c15ULL + (h << 6) +
             (h >> 2);
        h ^= static_cast<std::size_t>(key.texcoordIndex()) + 0x9e3779b97f4a7c15ULL + (h << 6) +
             (h >> 2);
        return h;
    }
};

void computeSmoothNormals(FMeshData& data) {
    for (auto& vertex : data.Vertices) {
        vertex.Normal = {0.0f, 0.0f, 0.0f};
    }

    for (std::size_t i = 0; i + 2 < data.Indices.size(); i += 3) {
        const auto i0 = data.Indices[i + 0];
        const auto i1 = data.Indices[i + 1];
        const auto i2 = data.Indices[i + 2];

        const glm::vec3 edge1 = data.Vertices[i1].Position - data.Vertices[i0].Position;
        const glm::vec3 edge2 = data.Vertices[i2].Position - data.Vertices[i0].Position;
        const glm::vec3 faceNormal = glm::cross(edge1, edge2);
        if (glm::dot(faceNormal, faceNormal) < 1e-20f) {
            continue;
        }

        const glm::vec3 n = glm::normalize(faceNormal);
        data.Vertices[i0].Normal += n;
        data.Vertices[i1].Normal += n;
        data.Vertices[i2].Normal += n;
    }

    for (auto& vertex : data.Vertices) {
        if (glm::dot(vertex.Normal, vertex.Normal) > 0.0f) {
            vertex.Normal = glm::normalize(vertex.Normal);
        } else {
            vertex.Normal = {0.0f, 1.0f, 0.0f};
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
                                std::unordered_map<FVertexKey, std::uint32_t, FVertexKeyHash>& unique,
                                const tinyobj::attrib_t& attrib, const tinyobj::index_t& index,
                                bool hasFileNormals, bool hasTexcoords) {
    const FVertexKey key{index.vertex_index, index.normal_index, index.texcoord_index};
    if (const auto found = unique.find(key); found != unique.end()) {
        return found->second;
    }

    FVertex vertex{};
    const auto vi = static_cast<std::size_t>(index.vertex_index) * 3u;
    vertex.Position = {
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
        vertex.Normal = (glm::dot(n, n) > 0.0f) ? glm::normalize(n) : glm::vec3{0, 1, 0};
    } else {
        vertex.Normal = {0.0f, 1.0f, 0.0f};
    }

    if (hasTexcoords && index.texcoord_index >= 0) {
        const auto ti = static_cast<std::size_t>(index.texcoord_index) * 2u;
        vertex.TexCoord = {
            attrib.texcoords[ti + 0],
            attrib.texcoords[ti + 1],
        };
    }

    const auto newIndex = static_cast<std::uint32_t>(data.Vertices.size());
    unique.emplace(key, newIndex);
    data.Vertices.push_back(vertex);
    return newIndex;
}

FMaterial materialFromTiny(const tinyobj::material_t& src) {
    FMaterial material;
    material.Shading = EMaterialShadingModel::BlinnPhong;
    material.Albedo = {src.diffuse[0], src.diffuse[1], src.diffuse[2]};
    material.Specular = {src.specular[0], src.specular[1], src.specular[2]};
    material.Alpha = src.dissolve;
    // Max/OBJ often exports low Ns; remap so highlights read clearly in Blinn-Phong.
    const float ns = std::max(src.shininess, 1.0f);
    material.Shininess = std::clamp((ns * ns * 0.25f) + (ns * 2.0f), 8.0f, 256.0f);

    // Heuristic metalness from MTL (no explicit metal map): strong Ks relative to Kd.
    const float kd = (material.Albedo.x + material.Albedo.y + material.Albedo.z) / 3.0f;
    const float ks = (material.Specular.x + material.Specular.y + material.Specular.z) / 3.0f;
    if (ks > 0.2f) {
        material.Metallic = std::clamp((ks - 0.15f) / 0.6f, 0.0f, 1.0f);
        // Painted metals in this asset use gray Ks; keep some metal even when Kd is dark.
        if (kd < 0.35f && ks >= 0.35f) {
            material.Metallic = std::max(material.Metallic, 0.65f);
        }
    }
    // Ensure specular floor so dielectrics still catch highlights.
    if (ks < 0.04f) {
        material.Specular = {0.04f, 0.04f, 0.04f};
    }

    material.SyncRoughnessFromShininess();
    material.bCastsShadows = material.Alpha >= 0.999f;
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
    data.Vertices.reserve(indexEstimate);

    std::unordered_map<FVertexKey, std::uint32_t, FVertexKeyHash> unique;
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

    if (data.Vertices.empty() || indicesByMaterial.empty()) {
        std::cerr << "Mesh has no geometry: " << path << '\n';
        return {};
    }

    if (!tinyMaterials.empty()) {
        data.Materials.reserve(tinyMaterials.size());
        data.AlbedoMapPaths.reserve(tinyMaterials.size());
        const std::filesystem::path objDir = std::filesystem::path(path).parent_path();
        for (const tinyobj::material_t& src : tinyMaterials) {
            data.Materials.push_back(materialFromTiny(src));
            if (!src.diffuse_texname.empty()) {
                data.AlbedoMapPaths.push_back((objDir / src.diffuse_texname).string());
            } else {
                data.AlbedoMapPaths.emplace_back();
            }
        }
    }

    data.Indices.reserve(indexEstimate);
    for (auto& [materialId, bucket] : indicesByMaterial) {
        if (bucket.empty()) {
            continue;
        }

        int slot = 0;
        if (materialId >= 0 && materialId < static_cast<int>(data.Materials.size())) {
            slot = materialId;
        } else if (!data.Materials.empty()) {
            slot = 0;
        }

        FMeshSection sub;
        sub.IndexOffset = static_cast<int>(data.Indices.size());
        sub.IndexCount = static_cast<int>(bucket.size());
        sub.MaterialIndex = slot;
        data.Indices.insert(data.Indices.end(), bucket.begin(), bucket.end());
        data.Submeshes.push_back(sub);
    }

    if (data.Materials.empty()) {
        data.Materials.push_back(FMaterial{});
        data.AlbedoMapPaths.emplace_back();
        for (FMeshSection& sub : data.Submeshes) {
            sub.MaterialIndex = 0;
        }
    }

    if (!hasFileNormals) {
        computeSmoothNormals(data);
    }

    std::cout << "OBJ '" << path << "': " << data.Vertices.size() << " verts, "
              << (data.Indices.size() / 3) << " tris, " << data.Submeshes.size() << " submeshes, "
              << data.Materials.size() << " materials\n";
    return data;
}

