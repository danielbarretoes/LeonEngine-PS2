#include "WorldEffectsRenderer.h"

#include "Effects/WorldEffects.h"
#include "Misc/Paths.h"
#include "OpenGLVertexAttrib.h"
#include "RendererLog.h"
#include "Texture2DResource.h"

#include <glad/glad.h>

namespace
{

	/** The shader's modes (world_effects.frag). */
	constexpr int32 MarkMode = 0;
	constexpr int32 TracerMode = 1;

} // namespace

FWorldEffectsRenderer::FWorldEffectsRenderer() = default;

FWorldEffectsRenderer::~FWorldEffectsRenderer() = default;

bool FWorldEffectsRenderer::Initialize(const FString& ShaderDirectory)
{
	if (!Shader.LoadFromFiles(FPaths::Combine(ShaderDirectory, TEXT("world_effects.vert")),
			FPaths::Combine(ShaderDirectory, TEXT("world_effects.frag"))))
	{
		UE_LOG(LogRenderer, Error, "Failed to load the world effects shaders from %s", *ShaderDirectory);
		return false;
	}

	TArray<uint8> Texels;
	BuildMaskTexels(Texels);
	Mask = MakeUnique<FTexture2DResource>(MaskSize, MaskSize, Texels.GetData());

	glGenVertexArrays(1, &Vao);
	glGenBuffers(1, &Vbo);
	glBindVertexArray(Vao);
	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FEffectVertex), GlAttribOffset(&FEffectVertex::Position));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FEffectVertex), GlAttribOffset(&FEffectVertex::TexCoord));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(FEffectVertex), GlAttribOffset(&FEffectVertex::Color));
	glBindVertexArray(0);
	return IsValid();
}

void FWorldEffectsRenderer::Shutdown()
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
	Mask.Reset();
	Shader.Destroy();
	Vertices.Empty();
}

EShaderReloadResult FWorldEffectsRenderer::ReloadShader(bool bForce)
{
	return bForce ? Shader.ForceReloadFromDisk() : Shader.ReloadFromDiskIfChanged();
}

void FWorldEffectsRenderer::Upload(const TArray<FEffectVertex>& InVertices) const
{
	glBindBuffer(GL_ARRAY_BUFFER, Vbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(InVertices.Num() * sizeof(FEffectVertex)),
		InVertices.GetData(), GL_DYNAMIC_DRAW);
}

void FWorldEffectsRenderer::Draw(int32 NumVertices, int32 Mode, const FMatrix& ViewProjection) const
{
	Shader.Bind();
	Shader.SetMat4("uViewProjection", ViewProjection);
	Shader.SetInt("uMask", 0);
	Shader.SetInt("uMode", Mode);
	Mask->Bind(0);
	glBindVertexArray(Vao);
	glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(NumVertices));
	glBindVertexArray(0);
}

void FWorldEffectsRenderer::DrawImpactMarks(const FImpactMarkPool& Marks, const FMatrix& ViewProjection)
{
	if (!IsValid() || Marks.IsEmpty())
	{
		return;
	}
	BuildImpactMarkVertices(Marks, Vertices);
	if (Vertices.Num() == 0)
	{
		return;
	}
	Upload(Vertices);
	// Multiplied into the surface, tested against it, not written, pulled toward the camera.
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_ZERO);
	glDepthMask(GL_FALSE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.0f, -2.0f);
	glDisable(GL_CULL_FACE);
	Draw(Vertices.Num(), MarkMode, ViewProjection);
	glEnable(GL_CULL_FACE);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

void FWorldEffectsRenderer::DrawTracers(
	const FTracerBatch& Tracers, const FMatrix& ViewProjection, const FVector& CameraLocation)
{
	if (!IsValid() || Tracers.IsEmpty())
	{
		return;
	}
	BuildTracerVertices(Tracers, CameraLocation, Vertices);
	if (Vertices.Num() == 0)
	{
		return;
	}
	Upload(Vertices);
	// Added to the scene's colour behind what is in front of it.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glDepthMask(GL_FALSE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
	Draw(Vertices.Num(), TracerMode, ViewProjection);
	glEnable(GL_CULL_FACE);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}
