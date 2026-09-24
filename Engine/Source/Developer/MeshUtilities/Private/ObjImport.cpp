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
    FVertexKey(int InPositionIndex, int InNormalIndex, int InTexcoordIndex)
        : PositionIndex(InPositionIndex), NormalIndex(InNormalIndex), TexcoordIndex(InTexcoordIndex) {}

    [[nodiscard]] bool operator==(const FVertexKey& Other) const {
        return PositionIndex == Other.PositionIndex && NormalIndex == Other.NormalIndex &&
               TexcoordIndex == Other.TexcoordIndex;
    }

    [[nodiscard]] int GetPositionIndex() const { return PositionIndex; }
    [[nodiscard]] int GetNormalIndex() const { return NormalIndex; }
    [[nodiscard]] int GetTexcoordIndex() const { return TexcoordIndex; }

private:
    int PositionIndex = 0;
    int NormalIndex = 0;
    int TexcoordIndex = 0;
};

struct FVertexKeyHash {
    std::size_t operator()(const FVertexKey& Key) const noexcept {
        auto H = static_cast<std::size_t>(Key.GetPositionIndex());
        H ^= static_cast<std::size_t>(Key.GetNormalIndex()) + 0x9e3779b97f4a7c15ULL + (H << 6) +
             (H >> 2);
        H ^= static_cast<std::size_t>(Key.GetTexcoordIndex()) + 0x9e3779b97f4a7c15ULL + (H << 6) +
             (H >> 2);
        return H;
    }
};

void ComputeSmoothNormals(FMeshData& Data) {
    for (auto& Vertex : Data.Vertices) {
        Vertex.Normal = {0.0f, 0.0f, 0.0f};
    }

    for (std::size_t I = 0; I + 2 < Data.Indices.size(); I += 3) {
        const auto I0 = Data.Indices[I + 0];
        const auto I1 = Data.Indices[I + 1];
        const auto I2 = Data.Indices[I + 2];

        const glm::vec3 Edge1 = Data.Vertices[I1].Position - Data.Vertices[I0].Position;
        const glm::vec3 Edge2 = Data.Vertices[I2].Position - Data.Vertices[I0].Position;
        const glm::vec3 FaceNormal = glm::cross(Edge1, Edge2);
        if (glm::dot(FaceNormal, FaceNormal) < 1e-20f) {
            continue;
        }

        const glm::vec3 N = glm::normalize(FaceNormal);
        Data.Vertices[I0].Normal += N;
        Data.Vertices[I1].Normal += N;
        Data.Vertices[I2].Normal += N;
    }

    for (auto& Vertex : Data.Vertices) {
        if (glm::dot(Vertex.Normal, Vertex.Normal) > 0.0f) {
            Vertex.Normal = glm::normalize(Vertex.Normal);
        } else {
            Vertex.Normal = {0.0f, 1.0f, 0.0f};
        }
    }
}

bool IndexInRange(int Index, std::size_t Count, int Components) {
    if (Index < 0) {
        return false;
    }
    const auto Needed = static_cast<std::size_t>(Index + 1) * static_cast<std::size_t>(Components);
    return Needed <= Count;
}

bool FaceIndicesValid(const tinyobj::attrib_t& Attrib, const tinyobj::index_t& Index,
                      bool bHasFileNormals, bool bHasTexcoords) {
    if (!IndexInRange(Index.vertex_index, Attrib.vertices.size(), 3)) {
        return false;
    }
    if (bHasFileNormals && Index.normal_index >= 0 &&
        !IndexInRange(Index.normal_index, Attrib.normals.size(), 3)) {
        return false;
    }
    if (bHasTexcoords && Index.texcoord_index >= 0 &&
        !IndexInRange(Index.texcoord_index, Attrib.texcoords.size(), 2)) {
        return false;
    }
    return true;
}

std::uint32_t GetOrCreateVertex(FMeshData& Data,
                                std::unordered_map<FVertexKey, std::uint32_t, FVertexKeyHash>& Unique,
                                const tinyobj::attrib_t& Attrib, const tinyobj::index_t& Index,
                                bool bHasFileNormals, bool bHasTexcoords) {
    const FVertexKey Key{Index.vertex_index, Index.normal_index, Index.texcoord_index};
    if (const auto Found = Unique.find(Key); Found != Unique.end()) {
        return Found->second;
    }

    FVertex Vertex{};
    const auto Vi = static_cast<std::size_t>(Index.vertex_index) * 3u;
    Vertex.Position = {
        Attrib.vertices[Vi + 0],
        Attrib.vertices[Vi + 1],
        Attrib.vertices[Vi + 2],
    };

    if (bHasFileNormals && Index.normal_index >= 0) {
        const auto Ni = static_cast<std::size_t>(Index.normal_index) * 3u;
        const glm::vec3 N{
            Attrib.normals[Ni + 0],
            Attrib.normals[Ni + 1],
            Attrib.normals[Ni + 2],
        };
        Vertex.Normal = (glm::dot(N, N) > 0.0f) ? glm::normalize(N) : glm::vec3{0, 1, 0};
    } else {
        Vertex.Normal = {0.0f, 1.0f, 0.0f};
    }

    if (bHasTexcoords && Index.texcoord_index >= 0) {
        const auto Ti = static_cast<std::size_t>(Index.texcoord_index) * 2u;
        Vertex.TexCoord = {
            Attrib.texcoords[Ti + 0],
            Attrib.texcoords[Ti + 1],
        };
    }

    const auto NewIndex = static_cast<std::uint32_t>(Data.Vertices.size());
    Unique.emplace(Key, NewIndex);
    Data.Vertices.push_back(Vertex);
    return NewIndex;
}

FMaterial MaterialFromTiny(const tinyobj::material_t& Src) {
    FMaterial Material;
    Material.Shading = EMaterialShadingModel::BlinnPhong;
    Material.Albedo = {Src.diffuse[0], Src.diffuse[1], Src.diffuse[2]};
    Material.Specular = {Src.specular[0], Src.specular[1], Src.specular[2]};
    Material.Alpha = Src.dissolve;
    // Max/OBJ often exports low Ns; remap so highlights read clearly in Blinn-Phong.
    const float Ns = std::max(Src.shininess, 1.0f);
    Material.Shininess = std::clamp((Ns * Ns * 0.25f) + (Ns * 2.0f), 8.0f, 256.0f);

    // Heuristic metalness from MTL (no explicit metal map): strong Ks relative to Kd.
    const float Kd = (Material.Albedo.x + Material.Albedo.y + Material.Albedo.z) / 3.0f;
    const float Ks = (Material.Specular.x + Material.Specular.y + Material.Specular.z) / 3.0f;
    if (Ks > 0.2f) {
        Material.Metallic = std::clamp((Ks - 0.15f) / 0.6f, 0.0f, 1.0f);
        // Painted metals in this asset use gray Ks; keep some metal even when Kd is dark.
        if (Kd < 0.35f && Ks >= 0.35f) {
            Material.Metallic = std::max(Material.Metallic, 0.65f);
        }
    }
    // Ensure specular floor so dielectrics still catch highlights.
    if (Ks < 0.04f) {
        Material.Specular = {0.04f, 0.04f, 0.04f};
    }

    Material.SyncRoughnessFromShininess();
    Material.bCastsShadows = Material.Alpha >= 0.999f;
    return Material;
}

} // namespace

FMeshData LoadObj(const std::string& Path) {
    tinyobj::ObjReaderConfig Config;
    Config.triangulate = true;
    Config.mtl_search_path = std::filesystem::path(Path).parent_path().string();

    tinyobj::ObjReader Reader;
    if (!Reader.ParseFromFile(Path, Config)) {
        if (!Reader.Error().empty()) {
            std::cerr << "tinyobjloader: " << Reader.Error() << '\n';
        }
        return {};
    }

    if (!Reader.Warning().empty()) {
        std::cerr << "tinyobjloader: " << Reader.Warning() << '\n';
    }

    const auto& Attrib = Reader.GetAttrib();
    const auto& Shapes = Reader.GetShapes();
    const auto& TinyMaterials = Reader.GetMaterials();

    std::size_t IndexEstimate = 0;
    for (const auto& Shape : Shapes) {
        IndexEstimate += Shape.mesh.indices.size();
    }

    FMeshData Data;
    Data.Vertices.reserve(IndexEstimate);

    std::unordered_map<FVertexKey, std::uint32_t, FVertexKeyHash> Unique;
    Unique.reserve(IndexEstimate);

    // materialId → triangle indices (grouped so each FMeshSection is contiguous).
    std::map<int, std::vector<std::uint32_t>> IndicesByMaterial;

    const bool bHasFileNormals = !Attrib.normals.empty();
    const bool bHasTexcoords = !Attrib.texcoords.empty();

    for (const auto& Shape : Shapes) {
        std::size_t IndexOffset = 0;
        for (std::size_t Face = 0; Face < Shape.mesh.num_face_vertices.size(); ++Face) {
            const unsigned int FaceVerts = Shape.mesh.num_face_vertices[Face];
            const int MaterialId =
                Face < Shape.mesh.material_ids.size() ? Shape.mesh.material_ids[Face] : -1;

            // Triangulated OBJ: expect 3 verts per face.
            if (FaceVerts != 3) {
                IndexOffset += FaceVerts;
                continue;
            }

            const tinyobj::index_t& I0 = Shape.mesh.indices[IndexOffset + 0];
            const tinyobj::index_t& I1 = Shape.mesh.indices[IndexOffset + 1];
            const tinyobj::index_t& I2 = Shape.mesh.indices[IndexOffset + 2];
            IndexOffset += 3;

            if (!FaceIndicesValid(Attrib, I0, bHasFileNormals, bHasTexcoords) ||
                !FaceIndicesValid(Attrib, I1, bHasFileNormals, bHasTexcoords) ||
                !FaceIndicesValid(Attrib, I2, bHasFileNormals, bHasTexcoords)) {
                std::cerr << "MeshData: skipping face with out-of-range indices in " << Path
                          << '\n';
                continue;
            }

            auto& Bucket = IndicesByMaterial[MaterialId];
            Bucket.push_back(
                GetOrCreateVertex(Data, Unique, Attrib, I0, bHasFileNormals, bHasTexcoords));
            Bucket.push_back(
                GetOrCreateVertex(Data, Unique, Attrib, I1, bHasFileNormals, bHasTexcoords));
            Bucket.push_back(
                GetOrCreateVertex(Data, Unique, Attrib, I2, bHasFileNormals, bHasTexcoords));
        }
    }

    if (Data.Vertices.empty() || IndicesByMaterial.empty()) {
        std::cerr << "Mesh has no geometry: " << Path << '\n';
        return {};
    }

    if (!TinyMaterials.empty()) {
        Data.Materials.reserve(TinyMaterials.size());
        Data.AlbedoMapPaths.reserve(TinyMaterials.size());
        const std::filesystem::path ObjDir = std::filesystem::path(Path).parent_path();
        for (const tinyobj::material_t& Src : TinyMaterials) {
            Data.Materials.push_back(MaterialFromTiny(Src));
            if (!Src.diffuse_texname.empty()) {
                Data.AlbedoMapPaths.push_back((ObjDir / Src.diffuse_texname).string());
            } else {
                Data.AlbedoMapPaths.emplace_back();
            }
        }
    }

    Data.Indices.reserve(IndexEstimate);
    for (auto& [materialId, bucket] : IndicesByMaterial) {
        if (bucket.empty()) {
            continue;
        }

        int Slot = 0;
        if (materialId >= 0 && materialId < static_cast<int>(Data.Materials.size())) {
            Slot = materialId;
        } else if (!Data.Materials.empty()) {
            Slot = 0;
        }

        FMeshSection Sub;
        Sub.IndexOffset = static_cast<int>(Data.Indices.size());
        Sub.IndexCount = static_cast<int>(bucket.size());
        Sub.MaterialIndex = Slot;
        Data.Indices.insert(Data.Indices.end(), bucket.begin(), bucket.end());
        Data.Submeshes.push_back(Sub);
    }

    if (Data.Materials.empty()) {
        Data.Materials.push_back(FMaterial{});
        Data.AlbedoMapPaths.emplace_back();
        for (FMeshSection& Sub : Data.Submeshes) {
            Sub.MaterialIndex = 0;
        }
    }

    if (!bHasFileNormals) {
        ComputeSmoothNormals(Data);
    }

    std::cout << "OBJ '" << Path << "': " << Data.Vertices.size() << " verts, "
              << (Data.Indices.size() / 3) << " tris, " << Data.Submeshes.size() << " submeshes, "
              << Data.Materials.size() << " materials\n";
    return Data;
}

