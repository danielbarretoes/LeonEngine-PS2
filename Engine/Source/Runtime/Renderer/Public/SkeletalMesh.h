#pragma once

#include "SkeletalAnimation.h"
#include "Material.h"
#include "RHIHandles.h"
#include <memory>
#include <vector>


/// GPU skinned mesh (VAO with bone indices/weights).
class USkeletalMesh {
public:
    USkeletalMesh() = default;
    ~USkeletalMesh();

    USkeletalMesh(const USkeletalMesh&) = delete;
    USkeletalMesh& operator=(const USkeletalMesh&) = delete;
    USkeletalMesh(USkeletalMesh&& other) noexcept;
    USkeletalMesh& operator=(USkeletalMesh&& other) noexcept;

    [[nodiscard]] static USkeletalMesh Upload(FSkeletalMeshData data);
    /// Skeleton / bounds / index count only (no VAO). Dedicated server path.
    [[nodiscard]] static USkeletalMesh CreateCpu(FSkeletalMeshData data);

    void Draw() const;

    [[nodiscard]] bool Valid() const {
        return indexCount_ > 0 && (cpuOnly_ || vao_ != kInvalidVertexArray);
    }
    [[nodiscard]] bool IsCpuOnly() const { return cpuOnly_; }
    [[nodiscard]] int IndexCount() const { return indexCount_; }
    [[nodiscard]] int TriangleCount() const { return indexCount_ / 3; }
    [[nodiscard]] const USkeleton& GetSkeleton() const { return skeleton_; }
    [[nodiscard]] const UAnimSequence& EmbeddedAnim() const { return embeddedAnim_; }
    [[nodiscard]] const glm::vec3& LocalMin() const { return localMin_; }
    [[nodiscard]] const glm::vec3& LocalMax() const { return localMax_; }
    [[nodiscard]] float FitUniformScale(float fitHeight) const;

    [[nodiscard]] FMaterial& GetMaterial() { return material_; }
    [[nodiscard]] const FMaterial& GetMaterial() const { return material_; }
    void SetMaterial(FMaterial material) { material_ = std::move(material); }

private:
    void Destroy();

    FRHIVertexArrayId vao_ = kInvalidVertexArray;
    FRHIBufferId vbo_ = kInvalidBuffer;
    FRHIBufferId ebo_ = kInvalidBuffer;
    int indexCount_ = 0;
    bool cpuOnly_ = false;
    USkeleton skeleton_{};
    UAnimSequence embeddedAnim_{};
    glm::vec3 localMin_{0.0f};
    glm::vec3 localMax_{0.0f};
    FMaterial material_{};
};

