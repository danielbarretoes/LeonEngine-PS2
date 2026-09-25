#include "LineBatchRenderer.h"

#include "Misc/Paths.h"
#include "OpenGLVertexAttrib.h"
#include "RendererLog.h"

#include <glad/glad.h>

bool FLineBatchRenderer::Initialize()
{
	// The engine's shaders (UE: /Engine/Shaders).
	const FString Vert = FPaths::Combine(FPaths::EngineDir(), TEXT("Shaders/debug_line.vert"));
	const FString Frag = FPaths::Combine(FPaths::EngineDir(), TEXT("Shaders/debug_line.frag"));
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
	glVertexAttribPointer(
		0, 3, GL_FLOAT, GL_FALSE, sizeof(FDebugDraw::FLineVertex), GlAttribOffset(&FDebugDraw::FLineVertex::Position));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(
		1, 3, GL_FLOAT, GL_FALSE, sizeof(FDebugDraw::FLineVertex), GlAttribOffset(&FDebugDraw::FLineVertex::Color));
	glBindVertexArray(0);
	return true;
}

EShaderReloadResult FLineBatchRenderer::ReloadShader(bool bForce)
{
	return bForce ? Shader.ForceReloadFromDisk() : Shader.ReloadFromDiskIfChanged();
}

void FLineBatchRenderer::Shutdown()
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
}

void FLineBatchRenderer::Flush(const FDebugDraw& Batch, const FMatrix& ViewProjection, bool bDepthTest) const
{
	const TArray<FDebugDraw::FLineVertex>& Vertices = Batch.GetVertices();
	if (!IsValid() || Vertices.Num() == 0)
	{
		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(Vertices.Num() * sizeof(FDebugDraw::FLineVertex)),
		Vertices.GetData(), GL_DYNAMIC_DRAW);

	glDisable(GL_BLEND);
	if (bDepthTest)
	{
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
	}
	else
	{
		glDisable(GL_DEPTH_TEST);
	}

	Shader.Bind();
	Shader.SetMat4("uViewProjection", ViewProjection);
	glBindVertexArray(Vao);
	glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(Vertices.Num()));
	glBindVertexArray(0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
}
