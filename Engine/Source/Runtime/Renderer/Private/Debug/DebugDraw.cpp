#include "Debug/DebugDraw.h"

#include "Misc/Paths.h"
#include "OpenGLVertexAttrib.h"
#include "RendererLog.h"

#include <glad/glad.h>

namespace
{
	/** The 12 edges of a box whose corners are numbered like AddAabb's. */
	constexpr int32 BoxEdges[12][2] = {
		{0, 1},
		{1, 2},
		{2, 3},
		{3, 0},
		{4, 5},
		{5, 6},
		{6, 7},
		{7, 4},
		{0, 4},
		{1, 5},
		{2, 6},
		{3, 7},
	};
} // namespace

bool FDebugDraw::Initialize(const FString& /*ShaderDirectory*/)
{
	const FString Vert = FPaths::ResolveLegacyContentPath("assets/Shaders/debug_line.vert");
	const FString Frag = FPaths::ResolveLegacyContentPath("assets/Shaders/debug_line.frag");
	if (!Shader.LoadFromFiles(Vert, Frag))
	{
		UE_LOG(LogRenderer, Error, "Failed to load debug line shaders");
		return false;
	}

	glGenVertexArrays(1, &Vao);
	glGenBuffers(1, &Vbo);
	glBindVertexArray(Vao);
	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FLineVertex), GlAttribOffset(&FLineVertex::Position));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FLineVertex), GlAttribOffset(&FLineVertex::Color));
	glBindVertexArray(0);
	return true;
}

EShaderReloadResult FDebugDraw::ReloadShader(bool bForce)
{
	return bForce ? Shader.ForceReloadFromDisk() : Shader.ReloadFromDiskIfChanged();
}

void FDebugDraw::Shutdown()
{
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
	Shader.Destroy();
	Vertices.Empty();
}

void FDebugDraw::Clear()
{
	Vertices.Reset();
}

void FDebugDraw::AddLine(const FVector& A, const FVector& B, const FLinearColor& InColor)
{
	const FVector Color(InColor.R, InColor.G, InColor.B);
	Vertices.Add(FLineVertex{A, Color});
	Vertices.Add(FLineVertex{B, Color});
}

void FDebugDraw::AddArrow(
	const FVector& From, const FVector& To, const FLinearColor& InColor, float HeadLength, float HeadWidth)
{
	AddLine(From, To, InColor);

	const FVector Shaft = To - From;
	const float Len = Shaft.Size();
	if (Len < 1.0e-2f)
	{
		return;
	}
	const FVector Dir = Shaft / Len;
	FVector Side = Dir ^ FVector(0.0f, 1.0f, 0.0f);
	if ((Side | Side) < 1.0e-6f)
	{
		Side = Dir ^ FVector(1.0f, 0.0f, 0.0f);
	}
	Side = Side.GetUnsafeNormal() * HeadWidth;
	const FVector Back = To - (Dir * HeadLength);
	AddLine(To, Back + Side, InColor);
	AddLine(To, Back - Side, InColor);
}

void FDebugDraw::AddAabb(const FVector& WorldMin, const FVector& WorldMax, const FLinearColor& InColor)
{
	const FVector& Mn = WorldMin;
	const FVector& Mx = WorldMax;
	const FVector C[8] = {
		FVector(Mn.X, Mn.Y, Mn.Z),
		FVector(Mx.X, Mn.Y, Mn.Z),
		FVector(Mx.X, Mx.Y, Mn.Z),
		FVector(Mn.X, Mx.Y, Mn.Z),
		FVector(Mn.X, Mn.Y, Mx.Z),
		FVector(Mx.X, Mn.Y, Mx.Z),
		FVector(Mx.X, Mx.Y, Mx.Z),
		FVector(Mn.X, Mx.Y, Mx.Z),
	};
	for (const auto& Edge : BoxEdges)
	{
		AddLine(C[Edge[0]], C[Edge[1]], InColor);
	}
}

void FDebugDraw::AddAxes(const FVector& Origin, float Size)
{
	AddLine(Origin, Origin + FVector(Size, 0.0f, 0.0f), FLinearColor(1.0f, 0.2f, 0.2f));
	AddLine(Origin, Origin + FVector(0.0f, Size, 0.0f), FLinearColor(0.2f, 1.0f, 0.2f));
	AddLine(Origin, Origin + FVector(0.0f, 0.0f, Size), FLinearColor(0.2f, 0.4f, 1.0f));
}

void FDebugDraw::AddLightFrustum(const FMatrix& LightSpace, const FLinearColor& InColor)
{
	const FMatrix Inv = LightSpace.Inverse();
	const FVector Ndc[8] = {
		FVector(-1.0f, -1.0f, -1.0f),
		FVector(1.0f, -1.0f, -1.0f),
		FVector(1.0f, 1.0f, -1.0f),
		FVector(-1.0f, 1.0f, -1.0f),
		FVector(-1.0f, -1.0f, 1.0f),
		FVector(1.0f, -1.0f, 1.0f),
		FVector(1.0f, 1.0f, 1.0f),
		FVector(-1.0f, 1.0f, 1.0f),
	};

	FVector World[8];
	for (int32 I = 0; I < 8; ++I)
	{
		FVector4 P = Inv.TransformFVector4(FVector4(Ndc[I], 1.0f));
		if (FMath::Abs(P.W) > 1e-6f)
		{
			P = P / P.W;
		}
		World[I] = FVector(P.X, P.Y, P.Z);
	}

	for (const auto& Edge : BoxEdges)
	{
		AddLine(World[Edge[0]], World[Edge[1]], InColor);
	}
}

void FDebugDraw::Flush(const FMatrix& ViewProjection) const
{
	if (!IsValid() || Vertices.Num() == 0)
	{
		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(Vertices.Num() * sizeof(FLineVertex)), Vertices.GetData(),
		GL_DYNAMIC_DRAW);

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	Shader.Bind();
	Shader.SetMat4("uViewProjection", ViewProjection);
	glBindVertexArray(Vao);
	glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(Vertices.Num()));
	glBindVertexArray(0);

	glDepthFunc(GL_LESS);
}
