#pragma once

#include <glm/vec3.hpp>

#include "Material.h"
#include "MeshData.h"
#include "RHIHandles.h"
#include <vector>

namespace leon {

/// GPU static mesh resource (Unreal-style StaticMesh; VAO/VBO/EBO + optional MTL).
class StaticMesh {
public:
    StaticMesh() = default;
    ~StaticMesh();

    StaticMesh(const StaticMesh&) = delete;
    StaticMesh& operator=(const StaticMesh&) = delete;
    StaticMesh(StaticMesh&& other) noexcept;
    StaticMesh& operator=(StaticMesh&& other) noexcept;

    [[nodiscard]] static StaticMesh Upload(const MeshData& data);
    /// Bounds + materials only (no VAO). For dedicated / headless simulation.
    [[nodiscard]] static StaticMesh CreateCpu(const MeshData& data);

    void Draw() const;
    void DrawSubMesh(std::size_t subMeshIndex) const;

    [[nodiscard]] bool Valid() const {
        return indexCount_ > 0 && (cpuOnly_ || vao_ != rhi::kInvalidVertexArray);
    }
    [[nodiscard]] bool IsCpuOnly() const { return cpuOnly_; }
    [[nodiscard]] int IndexCount() const { return indexCount_; }
    [[nodiscard]] int TriangleCount() const { return indexCount_ / 3; }
    [[nodiscard]] const glm::vec3& LocalMin() const { return localMin_; }
    [[nodiscard]] const glm::vec3& LocalMax() const { return localMax_; }
    [[nodiscard]] const std::vector<SubMesh>& Submeshes() const { return submeshes_; }
    [[nodiscard]] const std::vector<Material>& Materials() const { return materials_; }
    [[nodiscard]] bool HasMaterials() const { return !materials_.empty(); }
    /// CPU copy retained for lightmap bake / editor tools (empty if upload had no data).
    [[nodiscard]] const MeshData& CpuData() const { return cpuData_; }
    [[nodiscard]] bool HasCpuData() const { return !cpuData_.empty(); }

private:
    void Destroy();

    rhi::RHIVertexArrayId vao_ = rhi::kInvalidVertexArray;
    rhi::RHIBufferId vbo_ = rhi::kInvalidBuffer;
    rhi::RHIBufferId ebo_ = rhi::kInvalidBuffer;
    int indexCount_ = 0;
    bool cpuOnly_ = false;
    glm::vec3 localMin_{0.0f};
    glm::vec3 localMax_{0.0f};
    std::vector<SubMesh> submeshes_;
    std::vector<Material> materials_;
    MeshData cpuData_{};
};

} // namespace leon
