#include <glad/glad.h>
#include <glm/common.hpp>

#include <algorithm>
#include <leon/render/GlAttrib.h>
#include <leon/render/SkeletalMesh.h>
#include <utility>

namespace leon {

SkeletalMesh::~SkeletalMesh() {
    Destroy();
}

SkeletalMesh::SkeletalMesh(SkeletalMesh&& other) noexcept
    : vao_(other.vao_), vbo_(other.vbo_), ebo_(other.ebo_), indexCount_(other.indexCount_),
      cpuOnly_(other.cpuOnly_), skeleton_(std::move(other.skeleton_)),
      embeddedAnim_(std::move(other.embeddedAnim_)), localMin_(other.localMin_),
      localMax_(other.localMax_), material_(std::move(other.material_)) {
    other.vao_ = 0;
    other.vbo_ = 0;
    other.ebo_ = 0;
    other.indexCount_ = 0;
    other.cpuOnly_ = false;
}

SkeletalMesh& SkeletalMesh::operator=(SkeletalMesh&& other) noexcept {
    if (this != &other) {
        Destroy();
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        ebo_ = other.ebo_;
        indexCount_ = other.indexCount_;
        cpuOnly_ = other.cpuOnly_;
        skeleton_ = std::move(other.skeleton_);
        embeddedAnim_ = std::move(other.embeddedAnim_);
        localMin_ = other.localMin_;
        localMax_ = other.localMax_;
        material_ = std::move(other.material_);
        other.vao_ = 0;
        other.vbo_ = 0;
        other.ebo_ = 0;
        other.indexCount_ = 0;
        other.cpuOnly_ = false;
    }
    return *this;
}

SkeletalMesh SkeletalMesh::CreateCpu(SkeletalMeshData data) {
    SkeletalMesh mesh;
    if (data.empty()) {
        return mesh;
    }
    mesh.skeleton_ = std::move(data.skeleton);
    mesh.embeddedAnim_ = std::move(data.embeddedAnim);
    mesh.localMin_ = data.localMin;
    mesh.localMax_ = data.localMax;
    mesh.indexCount_ = static_cast<int>(data.indices.size());
    mesh.cpuOnly_ = true;
    return mesh;
}

SkeletalMesh SkeletalMesh::Upload(SkeletalMeshData data) {
    SkeletalMesh mesh;
    if (data.empty()) {
        return mesh;
    }

    mesh.skeleton_ = std::move(data.skeleton);
    mesh.embeddedAnim_ = std::move(data.embeddedAnim);
    mesh.localMin_ = data.localMin;
    mesh.localMax_ = data.localMax;

    glGenVertexArrays(1, &mesh.vao_);
    glGenBuffers(1, &mesh.vbo_);
    glGenBuffers(1, &mesh.ebo_);

    glBindVertexArray(mesh.vao_);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.vertices.size() * sizeof(SkeletalVertex)),
                 data.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.indices.size() * sizeof(std::uint32_t)),
                 data.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkeletalVertex),
                          GlAttribOffset(&SkeletalVertex::position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SkeletalVertex),
                          GlAttribOffset(&SkeletalVertex::normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SkeletalVertex),
                          GlAttribOffset(&SkeletalVertex::texCoord));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(SkeletalVertex),
                          GlAttribOffset(&SkeletalVertex::tangent));

    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, 4, GL_INT, sizeof(SkeletalVertex),
                           GlAttribOffset(&SkeletalVertex::boneIndices));

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(SkeletalVertex),
                          GlAttribOffset(&SkeletalVertex::boneWeights));

    glBindVertexArray(0);
    mesh.indexCount_ = static_cast<int>(data.indices.size());
    return mesh;
}

void SkeletalMesh::Draw() const {
    if (!Valid() || cpuOnly_ || vao_ == 0) {
        return;
    }
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

float SkeletalMesh::FitUniformScale(float fitHeight) const {
    if (fitHeight <= 0.0f) {
        return 1.0f;
    }
    const float height = std::max((localMax_ - localMin_).y, 0.001f);
    return fitHeight / height;
}

void SkeletalMesh::Destroy() {
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
}

} // namespace leon
