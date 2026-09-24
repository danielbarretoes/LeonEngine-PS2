#pragma once

#include <glm/vec3.hpp>

#include "Material.h"
#include "MeshData.h"
#include "RHIHandles.h"
#include <vector>


/// GPU static mesh resource (Unreal-style UStaticMesh; VAO/VBO/EBO + optional MTL).
class UStaticMesh {
public:
    UStaticMesh() = default;
    ~UStaticMesh();

    UStaticMesh(const UStaticMesh&) = delete;
    UStaticMesh& operator=(const UStaticMesh&) = delete;
    UStaticMesh(UStaticMesh&& other) noexcept;
    UStaticMesh& operator=(UStaticMesh&& other) noexcept;

    [[nodiscard]] static UStaticMesh Upload(const FMeshData& data);
    /// Bounds + materials only (no VAO). For dedicated / headless simulation.
    [[nodiscard]] static UStaticMesh CreateCpu(const FMeshData& data);

    void Draw() const;
    void DrawSubMesh(std::size_t subMeshIndex) const;

    [[nodiscard]] bool Valid() const {
        return indexCount_ > 0 && (cpuOnly_ || vao_ != kInvalidVertexArray);
    }
    [[nodiscard]] bool IsCpuOnly() const { return cpuOnly_; }
    [[nodiscard]] int IndexCount() const { return indexCount_; }
    [[nodiscard]] int TriangleCount() const { return indexCount_ / 3; }
    [[nodiscard]] const glm::vec3& LocalMin() const { return localMin_; }
    [[nodiscard]] const glm::vec3& LocalMax() const { return localMax_; }
    [[nodiscard]] const std::vector<FMeshSection>& Submeshes() const { return submeshes_; }
    [[nodiscard]] const std::vector<FMaterial>& Materials() const { return materials_; }
    [[nodiscard]] bool HasMaterials() const { return !materials_.empty(); }
    /// CPU copy retained for lightmap bake / editor tools (empty if upload had no data).
    [[nodiscard]] const FMeshData& CpuData() const { return cpuData_; }
    [[nodiscard]] bool HasCpuData() const { return !cpuData_.empty(); }

private:
    void Destroy();

    FRHIVertexArrayId vao_ = kInvalidVertexArray;
    FRHIBufferId vbo_ = kInvalidBuffer;
    FRHIBufferId ebo_ = kInvalidBuffer;
    int indexCount_ = 0;
    bool cpuOnly_ = false;
    glm::vec3 localMin_{0.0f};
    glm::vec3 localMax_{0.0f};
    std::vector<FMeshSection> submeshes_;
    std::vector<FMaterial> materials_;
    FMeshData cpuData_{};
};

