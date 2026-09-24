#include <glad/glad.h>
#include <glm/common.hpp>

#include <algorithm>
#include "OpenGLVertexAttrib.h"
#include "SkeletalMesh.h"
#include <utility>


USkeletalMesh::~USkeletalMesh() {
    Destroy();
}

USkeletalMesh::USkeletalMesh(USkeletalMesh&& Other) noexcept
    : Vao(Other.Vao), Vbo(Other.Vbo), Ebo(Other.Ebo), IndexCount(Other.IndexCount),
      bCpuOnly(Other.bCpuOnly), Skeleton(std::move(Other.Skeleton)),
      EmbeddedAnim(std::move(Other.EmbeddedAnim)), LocalMin(Other.LocalMin),
      LocalMax(Other.LocalMax), Material(std::move(Other.Material)) {
    Other.Vao = 0;
    Other.Vbo = 0;
    Other.Ebo = 0;
    Other.IndexCount = 0;
    Other.bCpuOnly = false;
}

USkeletalMesh& USkeletalMesh::operator=(USkeletalMesh&& Other) noexcept {
    if (this != &Other) {
        Destroy();
        Vao = Other.Vao;
        Vbo = Other.Vbo;
        Ebo = Other.Ebo;
        IndexCount = Other.IndexCount;
        bCpuOnly = Other.bCpuOnly;
        Skeleton = std::move(Other.Skeleton);
        EmbeddedAnim = std::move(Other.EmbeddedAnim);
        LocalMin = Other.LocalMin;
        LocalMax = Other.LocalMax;
        Material = std::move(Other.Material);
        Other.Vao = 0;
        Other.Vbo = 0;
        Other.Ebo = 0;
        Other.IndexCount = 0;
        Other.bCpuOnly = false;
    }
    return *this;
}

USkeletalMesh USkeletalMesh::CreateCpu(FSkeletalMeshData Data) {
    USkeletalMesh Mesh;
    if (Data.empty()) {
        return Mesh;
    }
    Mesh.Skeleton = std::move(Data.Skeleton);
    Mesh.EmbeddedAnim = std::move(Data.EmbeddedAnim);
    Mesh.LocalMin = Data.LocalMin;
    Mesh.LocalMax = Data.LocalMax;
    Mesh.IndexCount = static_cast<int>(Data.Indices.size());
    Mesh.bCpuOnly = true;
    return Mesh;
}

USkeletalMesh USkeletalMesh::Upload(FSkeletalMeshData Data) {
    USkeletalMesh Mesh;
    if (Data.empty()) {
        return Mesh;
    }

    Mesh.Skeleton = std::move(Data.Skeleton);
    Mesh.EmbeddedAnim = std::move(Data.EmbeddedAnim);
    Mesh.LocalMin = Data.LocalMin;
    Mesh.LocalMax = Data.LocalMax;

    glGenVertexArrays(1, &Mesh.Vao);
    glGenBuffers(1, &Mesh.Vbo);
    glGenBuffers(1, &Mesh.Ebo);

    glBindVertexArray(Mesh.Vao);

    glBindBuffer(GL_ARRAY_BUFFER, Mesh.Vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(Data.Vertices.size() * sizeof(FSkeletalVertex)),
                 Data.Vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Mesh.Ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(Data.Indices.size() * sizeof(std::uint32_t)),
                 Data.Indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FSkeletalVertex),
                          GlAttribOffset(&FSkeletalVertex::Position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FSkeletalVertex),
                          GlAttribOffset(&FSkeletalVertex::Normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(FSkeletalVertex),
                          GlAttribOffset(&FSkeletalVertex::TexCoord));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(FSkeletalVertex),
                          GlAttribOffset(&FSkeletalVertex::Tangent));

    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, 4, GL_INT, sizeof(FSkeletalVertex),
                           GlAttribOffset(&FSkeletalVertex::BoneIndices));

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(FSkeletalVertex),
                          GlAttribOffset(&FSkeletalVertex::BoneWeights));

    glBindVertexArray(0);
    Mesh.IndexCount = static_cast<int>(Data.Indices.size());
    return Mesh;
}

void USkeletalMesh::Draw() const {
    if (!Valid() || bCpuOnly || Vao == 0) {
        return;
    }
    glBindVertexArray(Vao);
    glDrawElements(GL_TRIANGLES, IndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

float USkeletalMesh::FitUniformScale(float FitHeight) const {
    if (FitHeight <= 0.0f) {
        return 1.0f;
    }
    const float Height = std::max((LocalMax - LocalMin).y, 0.001f);
    return FitHeight / Height;
}

void USkeletalMesh::Destroy() {
    if (Ebo != 0) {
        glDeleteBuffers(1, &Ebo);
        Ebo = 0;
    }
    if (Vbo != 0) {
        glDeleteBuffers(1, &Vbo);
        Vbo = 0;
    }
    if (Vao != 0) {
        glDeleteVertexArrays(1, &Vao);
        Vao = 0;
    }
    IndexCount = 0;
}

