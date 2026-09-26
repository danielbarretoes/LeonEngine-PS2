#include "WorldEffectsRenderer.h"

#include "Effects/WorldEffects.h"
#include "Misc/Paths.h"
#include "OpenGLVertexAttrib.h"
#include "RendererLog.h"
#include "Texture2DResource.h"

#include <glad/glad.h>

namespace
{

	static_assert(sizeof(FWorldEffectsRenderer::FEffectVertex) == 9 * sizeof(float), "Tightly packed effect vertex");

	/** How far a mark sits off its surface, cm (with the polygon offset, against z-fighting). */
	constexpr float MarkLift = 0.1f;

	/** The golden angle, radians: consecutive marks turn by it, so no two neighbours look alike. */
	constexpr float GoldenAngle = 2.39996323f;

	/** The side of the mask texture, texels. */
	constexpr int32 MaskSize = 32;

	/** The shader's modes (world_effects.frag). */
	constexpr int32 MarkMode = 0;
	constexpr int32 TracerMode = 1;

	/** Two triangles over the quad Centre +- AxisU +- AxisV. */
	void AddQuad(TArray<FWorldEffectsRenderer::FEffectVertex>& Out, const FVector& Centre, const FVector& AxisU,
		const FVector& AxisV, const FLinearColor& Color)
	{
		const FVector Corners[4] = {
			Centre - AxisU - AxisV, Centre + AxisU - AxisV, Centre + AxisU + AxisV, Centre - AxisU + AxisV};
		const FVector2D UVs[4] = {
			FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 1.0f)};
		constexpr int32 Order[6] = {0, 1, 2, 0, 2, 3};
		for (const int32 Corner : Order)
		{
			Out.Add({Corners[Corner], UVs[Corner], Color});
		}
	}

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

	// A soft round spot: 1 inside a quarter of the side, down to 0 at the rim (smoothstep).
	TArray<uint8> Texels;
	Texels.SetNumZeroed(MaskSize * MaskSize * 4);
	for (int32 Y = 0; Y < MaskSize; ++Y)
	{
		for (int32 X = 0; X < MaskSize; ++X)
		{
			const float U = ((static_cast<float>(X) + 0.5f) / static_cast<float>(MaskSize)) - 0.5f;
			const float V = ((static_cast<float>(Y) + 0.5f) / static_cast<float>(MaskSize)) - 0.5f;
			const float Radius = FMath::Sqrt((U * U) + (V * V));
			const float T = FMath::Clamp((Radius - 0.25f) / 0.25f, 0.0f, 1.0f);
			const float Value = 1.0f - (T * T * (3.0f - (2.0f * T)));
			const uint8 Byte = static_cast<uint8>(FMath::RoundToInt(Value * 255.0f));
			uint8* Texel = &Texels[((Y * MaskSize) + X) * 4];
			Texel[0] = Byte;
			Texel[1] = Byte;
			Texel[2] = Byte;
			Texel[3] = 255;
		}
	}
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

void FWorldEffectsRenderer::BuildImpactMarkVertices(const FImpactMarkPool& Marks, TArray<FEffectVertex>& OutVertices)
{
	OutVertices.Reset();
	for (const FImpactMark& Mark : Marks.GetMarks())
	{
		const float Opacity = Mark.GetOpacity();
		if (!Mark.bActive || Opacity <= 0.0f || Mark.Size <= 0.0f)
		{
			continue;
		}
		const FVector Normal = Mark.Normal.GetSafeNormal();
		if (Normal.IsNearlyZero())
		{
			continue;
		}
		// Two axes across the normal, turned by the mark's serial.
		const FVector Reference = FMath::Abs(Normal.Z) < 0.9f ? FVector(0.0f, 0.0f, 1.0f) : FVector(1.0f, 0.0f, 0.0f);
		const FVector AxisU = FVector::CrossProduct(Normal, Reference).GetSafeNormal();
		const FVector AxisV = FVector::CrossProduct(Normal, AxisU);
		const float Angle = static_cast<float>(Mark.Serial % 64u) * GoldenAngle;
		const float Cos = FMath::Cos(Angle);
		const float Sin = FMath::Sin(Angle);
		const float Half = Mark.Size * 0.5f;
		const FVector TurnedU = ((AxisU * Cos) + (AxisV * Sin)) * Half;
		const FVector TurnedV = ((AxisV * Cos) - (AxisU * Sin)) * Half;
		const FLinearColor Color(Mark.Color.R, Mark.Color.G, Mark.Color.B, Opacity);
		AddQuad(OutVertices, Mark.Location + (Normal * MarkLift), TurnedU, TurnedV, Color);
	}
}

void FWorldEffectsRenderer::BuildTracerVertices(
	const FTracerBatch& Tracers, const FVector& CameraLocation, TArray<FEffectVertex>& OutVertices)
{
	OutVertices.Reset();
	for (const FTracer& Tracer : Tracers.GetTracers())
	{
		const float Opacity = Tracer.GetOpacity();
		const FVector Along = Tracer.End - Tracer.Start;
		if (Opacity <= 0.0f || Along.IsNearlyZero())
		{
			continue;
		}
		// Across the segment and the line of sight, so the ribbon faces the camera.
		const FVector Middle = (Tracer.Start + Tracer.End) * 0.5f;
		FVector Side = FVector::CrossProduct(Along, CameraLocation - Middle).GetSafeNormal();
		if (Side.IsNearlyZero())
		{
			continue;
		}
		Side *= Tracer.Width * 0.5f;
		const FLinearColor Color(Tracer.Color.R, Tracer.Color.G, Tracer.Color.B, Opacity);
		const FVector HalfAlong = Along * 0.5f;
		// U runs along the tracer, V across it: the mask's middle column, bright at the centre line.
		const int32 First = OutVertices.Num();
		AddQuad(OutVertices, Middle, HalfAlong, Side, Color);
		for (int32 Index = First; Index < OutVertices.Num(); ++Index)
		{
			OutVertices[Index].TexCoord.X = 0.5f;
		}
	}
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
