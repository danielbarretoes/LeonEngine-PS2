#include "SkeletalMeshRenderData.h"

#include "Engine/SkeletalMesh.h"
#include "OpenGLVertexAttrib.h"

#include <glad/glad.h>

FSkeletalMeshRenderData::FSkeletalMeshRenderData(const USkeletalMesh& Mesh)
{
	const TArray<FSkeletalVertex>& Vertices = Mesh.GetVertices();
	const TArray<uint32>& Indices = Mesh.GetIndices();
	if (Vertices.Num() == 0 || Indices.Num() == 0)
	{
		return;
	}

	glGenVertexArrays(1, &Vao);
	glGenBuffers(1, &Vbo);
	glGenBuffers(1, &Ebo);

	glBindVertexArray(Vao);

	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(Vertices.Num() * sizeof(FSkeletalVertex)), Vertices.GetData(),
		GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(Indices.Num() * sizeof(uint32)), Indices.GetData(),
		GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(
		0, 3, GL_FLOAT, GL_FALSE, sizeof(FSkeletalVertex), GlAttribOffset(&FSkeletalVertex::Position));

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FSkeletalVertex), GlAttribOffset(&FSkeletalVertex::Normal));

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(
		2, 2, GL_FLOAT, GL_FALSE, sizeof(FSkeletalVertex), GlAttribOffset(&FSkeletalVertex::TexCoord));

	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(FSkeletalVertex), GlAttribOffset(&FSkeletalVertex::Tangent));

	glEnableVertexAttribArray(4);
	glVertexAttribIPointer(4, 4, GL_INT, sizeof(FSkeletalVertex), GlAttribOffset(&FSkeletalVertex::BoneIndices));

	glEnableVertexAttribArray(5);
	glVertexAttribPointer(
		5, 4, GL_FLOAT, GL_FALSE, sizeof(FSkeletalVertex), GlAttribOffset(&FSkeletalVertex::BoneWeights));

	glBindVertexArray(0);
	IndexCount = Indices.Num();
}

FSkeletalMeshRenderData::~FSkeletalMeshRenderData()
{
	if (Ebo != 0)
	{
		glDeleteBuffers(1, &Ebo);
	}
	if (Vbo != 0)
	{
		glDeleteBuffers(1, &Vbo);
	}
	if (Vao != 0)
	{
		glDeleteVertexArrays(1, &Vao);
	}
}

void FSkeletalMeshRenderData::Draw() const
{
	if (!Valid())
	{
		return;
	}
	glBindVertexArray(Vao);
	glDrawElements(GL_TRIANGLES, IndexCount, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}
