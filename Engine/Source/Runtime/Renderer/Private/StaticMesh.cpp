#include "StaticMesh.h"

#include "OpenGLVertexAttrib.h"

#include <glad/glad.h>

namespace
{

	[[nodiscard]] const void* GlIndexByteOffset(int32 IndexOffset) noexcept
	{
		const uint32* Base = nullptr;
		return static_cast<const void*>(Base + IndexOffset);
	}

	void ComputeLocalBounds(const FMeshData& Data, FVector& OutMin, FVector& OutMax)
	{
		OutMin = FVector(TNumericLimits<float>::Max());
		OutMax = FVector(TNumericLimits<float>::Lowest());
		for (const FVertex& Vertex : Data.Vertices)
		{
			OutMin = OutMin.ComponentMin(Vertex.Position);
			OutMax = OutMax.ComponentMax(Vertex.Position);
		}
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
	, Submeshes(MoveTemp(Other.Submeshes))
	, Materials(MoveTemp(Other.Materials))
	, CpuData(MoveTemp(Other.CpuData))
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
		Submeshes = MoveTemp(Other.Submeshes);
		Materials = MoveTemp(Other.Materials);
		CpuData = MoveTemp(Other.CpuData);
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

	ComputeLocalBounds(Data, Mesh.LocalMin, Mesh.LocalMax);
	Mesh.IndexCount = Data.Indices.Num();
	Mesh.Materials = Data.Materials;
	if (Data.Submeshes.Num() == 0)
	{
		Mesh.Submeshes.Add(FMeshSection{0, Mesh.IndexCount, 0});
	}
	else
	{
		Mesh.Submeshes = Data.Submeshes;
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

	ComputeLocalBounds(UploadData, Result.LocalMin, Result.LocalMax);

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
	Result.Materials = UploadData.Materials;

	if (UploadData.Submeshes.Num() == 0)
	{
		Result.Submeshes.Add(FMeshSection{0, Result.IndexCount, 0});
	}
	else
	{
		Result.Submeshes = UploadData.Submeshes;
	}
	Result.CpuData = MoveTemp(UploadData);
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

void UStaticMesh::DrawSubMesh(int32 SubMeshIndex) const
{
	if (!Valid() || bCpuOnly || Vao == 0 || !Submeshes.IsValidIndex(SubMeshIndex))
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
	Submeshes.Empty();
	Materials.Empty();
	CpuData = FMeshData();
}
