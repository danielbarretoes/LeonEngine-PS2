#include "StaticMesh.h"

#include "Migration/GlmInterop.h"
#include "OpenGLVertexAttrib.h"

#include <glad/glad.h>
#include <glm/common.hpp>

#include <cstdint>
#include <limits>
#include <utility>

namespace
{

	[[nodiscard]] const void* GlIndexByteOffset(int IndexOffset) noexcept
	{
		const std::uint32_t* Base = nullptr;
		return static_cast<const void*>(Base + IndexOffset);
	}

} // namespace

UStaticMesh::~UStaticMesh()
{
	Destroy();
}

UStaticMesh::UStaticMesh(UStaticMesh&& Other) noexcept
	: Vao(Other.Vao)
	, Vbo(Other.Vbo)
	, Ebo(Other.Ebo)
	, IndexCount(Other.IndexCount)
	, bCpuOnly(Other.bCpuOnly)
	, LocalMin(Other.LocalMin)
	, LocalMax(Other.LocalMax)
	, Submeshes(std::move(Other.Submeshes))
	, Materials(std::move(Other.Materials))
	, CpuData(std::move(Other.CpuData))
{
	Other.Vao = 0;
	Other.Vbo = 0;
	Other.Ebo = 0;
	Other.IndexCount = 0;
	Other.bCpuOnly = false;
}

UStaticMesh& UStaticMesh::operator=(UStaticMesh&& Other) noexcept
{
	if (this != &Other)
	{
		Destroy();
		Vao = Other.Vao;
		Vbo = Other.Vbo;
		Ebo = Other.Ebo;
		IndexCount = Other.IndexCount;
		bCpuOnly = Other.bCpuOnly;
		LocalMin = Other.LocalMin;
		LocalMax = Other.LocalMax;
		Submeshes = std::move(Other.Submeshes);
		Materials = std::move(Other.Materials);
		CpuData = std::move(Other.CpuData);
		Other.Vao = 0;
		Other.Vbo = 0;
		Other.Ebo = 0;
		Other.IndexCount = 0;
		Other.bCpuOnly = false;
	}
	return *this;
}

UStaticMesh UStaticMesh::CreateCpu(const FMeshData& Data)
{
	UStaticMesh Mesh;
	if (Data.IsEmpty())
	{
		return Mesh;
	}

	Mesh.LocalMin = glm::vec3(std::numeric_limits<float>::max());
	Mesh.LocalMax = glm::vec3(std::numeric_limits<float>::lowest());
	for (const FVertex& Vertex : Data.Vertices)
	{
		Mesh.LocalMin = glm::min(Mesh.LocalMin, ToGlm(Vertex.Position));
		Mesh.LocalMax = glm::max(Mesh.LocalMax, ToGlm(Vertex.Position));
	}
	Mesh.IndexCount = Data.Indices.Num();
	Mesh.Materials.assign(Data.Materials.GetData(), Data.Materials.GetData() + Data.Materials.Num());
	if (Data.Submeshes.Num() == 0)
	{
		Mesh.Submeshes.push_back(FMeshSection{0, Mesh.IndexCount, 0});
	}
	else
	{
		Mesh.Submeshes.assign(Data.Submeshes.GetData(), Data.Submeshes.GetData() + Data.Submeshes.Num());
	}
	Mesh.bCpuOnly = true;
	Mesh.CpuData = Data;
	return Mesh;
}

UStaticMesh UStaticMesh::Upload(const FMeshData& Data)
{
	UStaticMesh Result;
	if (Data.IsEmpty())
	{
		return Result;
	}

	FMeshData UploadData = Data;
	ComputeTangents(UploadData);

	Result.LocalMin = glm::vec3(std::numeric_limits<float>::max());
	Result.LocalMax = glm::vec3(std::numeric_limits<float>::lowest());
	for (const FVertex& Vertex : UploadData.Vertices)
	{
		Result.LocalMin = glm::min(Result.LocalMin, ToGlm(Vertex.Position));
		Result.LocalMax = glm::max(Result.LocalMax, ToGlm(Vertex.Position));
	}

	glGenVertexArrays(1, &Result.Vao);
	glGenBuffers(1, &Result.Vbo);
	glGenBuffers(1, &Result.Ebo);

	glBindVertexArray(Result.Vao);

	glBindBuffer(GL_ARRAY_BUFFER, Result.Vbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(UploadData.Vertices.Num() * sizeof(FVertex)),
		UploadData.Vertices.GetData(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Result.Ebo);
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
	Result.IndexCount = UploadData.Indices.Num();
	Result.Materials.assign(
		UploadData.Materials.GetData(), UploadData.Materials.GetData() + UploadData.Materials.Num());

	if (UploadData.Submeshes.Num() == 0)
	{
		Result.Submeshes.push_back(FMeshSection{0, Result.IndexCount, 0});
	}
	else
	{
		Result.Submeshes.assign(
			UploadData.Submeshes.GetData(), UploadData.Submeshes.GetData() + UploadData.Submeshes.Num());
	}
	Result.CpuData = std::move(UploadData);
	return Result;
}

void UStaticMesh::Draw() const
{
	if (!Valid() || bCpuOnly || Vao == 0)
	{
		return;
	}
	glBindVertexArray(Vao);
	glDrawElements(GL_TRIANGLES, IndexCount, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}

void UStaticMesh::DrawSubMesh(std::size_t SubMeshIndex) const
{
	if (!Valid() || bCpuOnly || Vao == 0 || SubMeshIndex >= Submeshes.size())
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

void UStaticMesh::Destroy()
{
	if (Ebo != 0)
	{
		glDeleteBuffers(1, &Ebo);
		Ebo = 0;
	}
	if (Vbo != 0)
	{
		glDeleteBuffers(1, &Vbo);
		Vbo = 0;
	}
	if (Vao != 0)
	{
		glDeleteVertexArrays(1, &Vao);
		Vao = 0;
	}
	IndexCount = 0;
	Submeshes.clear();
	Materials.clear();
	CpuData = {};
}
