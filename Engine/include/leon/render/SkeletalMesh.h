#pragma once

#include <leon/animation/SkeletalAnimation.h>
#include <leon/render/Material.h>
#include <leon/rhi/RHIHandles.h>
#include <memory>
#include <vector>

namespace leon {

/// GPU skinned mesh (VAO with bone indices/weights).
class SkeletalMesh {
public:
    SkeletalMesh() = default;
    ~SkeletalMesh();

    SkeletalMesh(const SkeletalMesh&) = delete;
    SkeletalMesh& operator=(const SkeletalMesh&) = delete;
    SkeletalMesh(SkeletalMesh&& other) noexcept;
    SkeletalMesh& operator=(SkeletalMesh&& other) noexcept;

    [[nodiscard]] static SkeletalMesh Upload(SkeletalMeshData data);
    /// Skeleton / bounds / index count only (no VAO). Dedicated server path.
    [[nodiscard]] static SkeletalMesh CreateCpu(SkeletalMeshData data);

    void Draw() const;

    [[nodiscard]] bool Valid() const {
        return indexCount_ > 0 && (cpuOnly_ || vao_ != rhi::kInvalidVertexArray);
    }
    [[nodiscard]] bool IsCpuOnly() const { return cpuOnly_; }
    [[nodiscard]] int IndexCount() const { return indexCount_; }
    [[nodiscard]] int TriangleCount() const { return indexCount_ / 3; }
    [[nodiscard]] const Skeleton& GetSkeleton() const { return skeleton_; }
    [[nodiscard]] const AnimSequence& EmbeddedAnim() const { return embeddedAnim_; }
    [[nodiscard]] const glm::vec3& LocalMin() const { return localMin_; }
    [[nodiscard]] const glm::vec3& LocalMax() const { return localMax_; }
    [[nodiscard]] float FitUniformScale(float fitHeight) const;

    [[nodiscard]] Material& GetMaterial() { return material_; }
    [[nodiscard]] const Material& GetMaterial() const { return material_; }
    void SetMaterial(Material material) { material_ = std::move(material); }

private:
    void Destroy();

    rhi::RHIVertexArrayId vao_ = rhi::kInvalidVertexArray;
    rhi::RHIBufferId vbo_ = rhi::kInvalidBuffer;
    rhi::RHIBufferId ebo_ = rhi::kInvalidBuffer;
    int indexCount_ = 0;
    bool cpuOnly_ = false;
    Skeleton skeleton_{};
    AnimSequence embeddedAnim_{};
    glm::vec3 localMin_{0.0f};
    glm::vec3 localMax_{0.0f};
    Material material_{};
};

} // namespace leon
