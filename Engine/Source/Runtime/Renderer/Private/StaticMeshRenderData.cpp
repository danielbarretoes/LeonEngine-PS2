#include "StaticMeshRenderData.h"

#include "Engine/StaticMesh.h"
#include "OpenGLVertexAttrib.h"

#include <glad/glad.h>

namespace
{

	[[nodiscard]] const void* GlIndexByteOffset(int32 IndexOffset) noexcept
	{
		const uint32* Base = nullptr;
		return static_cast<const void*>(Base + IndexOffset);
	}

} // namespace

FStaticMeshRenderData::FStaticMeshRenderData(const UStaticMesh& Mesh)
{
	if (!Mesh.HasValidRenderData())
	{
		return;
	}

	FMeshData UploadData;
	Mesh.GetLODResources().ToMeshData(UploadData);
	ComputeTangents(UploadData);

	glGenVertexArrays(1, &Vao);
	glGenBuffers(1, &Vbo);
	glGenBuffers(1, &Ebo);

	glBindVertexArray(Vao);

	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(UploadData.Vertices.Num() * sizeof(FVertex)),
		UploadData.Vertices.GetData(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(UploadData.Indices.Num() * sizeof(uint32)),
		UploadData.Indices.GetData(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FVertex), GlAttribOffset(&FVertex::Position));

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FVertex), GlAttribOffset(&FVertex::Normal));

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(FVertex), GlAttribOffset(&FVertex::TexCoord));

	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(FVertex), GlAttribOffset(&FVertex::Tangent));

	glBindVertexArray(0);
	IndexCount = UploadData.Indices.Num();
	Submeshes = Mesh.GetLODResources().Sections;
}

FStaticMeshRenderData::~FStaticMeshRenderData()
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

void FStaticMeshRenderData::Draw() const
{
	if (!Valid())
	{
		return;
	}
	glBindVertexArray(Vao);
	glDrawElements(GL_TRIANGLES, IndexCount, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}

void FStaticMeshRenderData::DrawSubMesh(int32 SubMeshIndex) const
{
	if (!Valid() || !Submeshes.IsValidIndex(SubMeshIndex))
	{
		return;
	}
	const FMeshSection& Sub = Submeshes[SubMeshIndex];
	if (Sub.IndexCount <= 0)
	{
		return;
	}
	glBindVertexArray(Vao);
	glDrawElements(GL_TRIANGLES, Sub.IndexCount, GL_UNSIGNED_INT, GlIndexByteOffset(Sub.IndexOffset));
	glBindVertexArray(0);
}
