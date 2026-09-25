#include "CanvasRenderer.h"

#include "CanvasTypes.h"
#include "Misc/Paths.h"
#include "OpenGLVertexAttrib.h"
#include "RenderMatrices.h"
#include "RendererLog.h"

#include <glad/glad.h>

bool FCanvasRenderer::Initialize()
{
	// The engine's shaders (UE: /Engine/Shaders).
	const FString Vert = FPaths::Combine(FPaths::EngineDir(), TEXT("Shaders/debug_overlay.vert"));
	const FString Frag = FPaths::Combine(FPaths::EngineDir(), TEXT("Shaders/debug_overlay.frag"));
	if (!Shader.LoadFromFiles(Vert, Frag))
	{
		UE_LOG(LogRenderer, Error, "Failed to load debug overlay shaders");
		return false;
	}

	glGenVertexArrays(1, &Vao);
	glGenBuffers(1, &Vbo);
	glBindVertexArray(Vao);
	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FCanvasVertex), GlAttribOffset(&FCanvasVertex::X));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(FCanvasVertex), GlAttribOffset(&FCanvasVertex::R));
	glBindVertexArray(0);
	return true;
}

void FCanvasRenderer::Shutdown()
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

EShaderReloadResult FCanvasRenderer::ReloadShader(bool bForce)
{
	return bForce ? Shader.ForceReloadFromDisk() : Shader.ReloadFromDiskIfChanged();
}

void FCanvasRenderer::Draw(const FCanvas& Canvas)
{
	const int32 FramebufferWidth = Canvas.GetSizeX();
	const int32 FramebufferHeight = Canvas.GetSizeY();
	if (!IsValid() || FramebufferWidth <= 0 || FramebufferHeight <= 0)
	{
		return;
	}

	TArray<FCanvasVertex> Tris;
	Canvas.GetTriangles(Tris);
	if (Tris.Num() <= 0)
	{
		return;
	}
	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glBufferData(
		GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(Tris.Num() * sizeof(FCanvasVertex)), Tris.GetData(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	const FMatrix Projection =
		MakePixelSpaceProjection(static_cast<float>(FramebufferWidth), static_cast<float>(FramebufferHeight));

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Shader.Bind();
	Shader.SetMat4("uProjection", Projection);
	glBindVertexArray(Vao);
	glDrawArrays(GL_TRIANGLES, 0, Tris.Num());
	glBindVertexArray(0);

	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}
