#include <glad/glad.h>
#include <glm/common.hpp>

#include <cstdint>
#include "OpenGLVertexAttrib.h"
#include "StaticMesh.h"
#include <limits>
#include <utility>

namespace {

[[nodiscard]] const void* glIndexByteOffset(int indexOffset) noexcept {
    const std::uint32_t* base = nullptr;
    return static_cast<const void*>(base + indexOffset);
}

} // namespace

UStaticMesh::~UStaticMesh() {
    Destroy();
}

UStaticMesh::UStaticMesh(UStaticMesh&& other) noexcept
    : vao_(other.vao_), vbo_(other.vbo_), ebo_(other.ebo_), indexCount_(other.indexCount_),
      cpuOnly_(other.cpuOnly_), localMin_(other.localMin_), localMax_(other.localMax_),
      submeshes_(std::move(other.submeshes_)), materials_(std::move(other.materials_)),
      cpuData_(std::move(other.cpuData_)) {
    other.vao_ = 0;
    other.vbo_ = 0;
    other.ebo_ = 0;
    other.indexCount_ = 0;
    other.cpuOnly_ = false;
}

UStaticMesh& UStaticMesh::operator=(UStaticMesh&& other) noexcept {
    if (this != &other) {
        Destroy();
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        ebo_ = other.ebo_;
        indexCount_ = other.indexCount_;
        cpuOnly_ = other.cpuOnly_;
        localMin_ = other.localMin_;
        localMax_ = other.localMax_;
        submeshes_ = std::move(other.submeshes_);
        materials_ = std::move(other.materials_);
        cpuData_ = std::move(other.cpuData_);
        other.vao_ = 0;
        other.vbo_ = 0;
        other.ebo_ = 0;
        other.indexCount_ = 0;
        other.cpuOnly_ = false;
    }
    return *this;
}

UStaticMesh UStaticMesh::CreateCpu(const FMeshData& data) {
    UStaticMesh mesh;
    if (data.empty()) {
        return mesh;
    }

    mesh.localMin_ = glm::vec3(std::numeric_limits<float>::max());
    mesh.localMax_ = glm::vec3(std::numeric_limits<float>::lowest());
    for (const FVertex& vertex : data.vertices) {
        mesh.localMin_ = glm::min(mesh.localMin_, vertex.position);
        mesh.localMax_ = glm::max(mesh.localMax_, vertex.position);
    }
    mesh.indexCount_ = static_cast<int>(data.indices.size());
    mesh.materials_ = data.materials;
    if (data.submeshes.empty()) {
        mesh.submeshes_.push_back(FMeshSection{0, mesh.indexCount_, 0});
    } else {
        mesh.submeshes_ = data.submeshes;
    }
    mesh.cpuOnly_ = true;
    mesh.cpuData_ = data;
    return mesh;
}

UStaticMesh UStaticMesh::Upload(const FMeshData& data) {
    UStaticMesh Result;
    if (data.empty()) {
        return Result;
    }

    FMeshData uploadData = data;
    ComputeTangents(uploadData);

    Result.localMin_ = glm::vec3(std::numeric_limits<float>::max());
    Result.localMax_ = glm::vec3(std::numeric_limits<float>::lowest());
    for (const FVertex& vertex : uploadData.vertices) {
        Result.localMin_ = glm::min(Result.localMin_, vertex.position);
        Result.localMax_ = glm::max(Result.localMax_, vertex.position);
    }

    glGenVertexArrays(1, &Result.vao_);
    glGenBuffers(1, &Result.vbo_);
    glGenBuffers(1, &Result.ebo_);

    glBindVertexArray(Result.vao_);

    glBindBuffer(GL_ARRAY_BUFFER, Result.vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(uploadData.vertices.size() * sizeof(FVertex)),
                 uploadData.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Result.ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(uploadData.indices.size() * sizeof(std::uint32_t)),
                 uploadData.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FVertex),
                          GlAttribOffset(&FVertex::position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FVertex),
                          GlAttribOffset(&FVertex::normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(FVertex),
                          GlAttribOffset(&FVertex::texCoord));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(FVertex),
                          GlAttribOffset(&FVertex::tangent));

    glBindVertexArray(0);
    Result.indexCount_ = static_cast<int>(uploadData.indices.size());
    Result.materials_ = uploadData.materials;

    if (uploadData.submeshes.empty()) {
        Result.submeshes_.push_back(FMeshSection{0, Result.indexCount_, 0});
    } else {
        Result.submeshes_ = uploadData.submeshes;
    }
    Result.cpuData_ = std::move(uploadData);
    return Result;
}

void UStaticMesh::Draw() const {
    if (!Valid() || cpuOnly_ || vao_ == 0) {
        return;
    }
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void UStaticMesh::DrawSubMesh(std::size_t subMeshIndex) const {
    if (!Valid() || cpuOnly_ || vao_ == 0 || subMeshIndex >= submeshes_.size()) {
        return;
    }
    const FMeshSection& sub = submeshes_[subMeshIndex];
    if (sub.indexCount <= 0) {
        return;
    }
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_INT,
                   glIndexByteOffset(sub.indexOffset));
    glBindVertexArray(0);
}

void UStaticMesh::Destroy() {
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    indexCount_ = 0;
    submeshes_.clear();
    materials_.clear();
    cpuData_ = {};
}

