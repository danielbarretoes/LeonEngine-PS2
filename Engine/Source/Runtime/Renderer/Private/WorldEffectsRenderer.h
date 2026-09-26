#pragma once

#include "CoreMinimal.h"
#include "Shader.h"

class FImpactMarkPool;
class FTexture2DResource;
class FTracerBatch;

/**
 * Draws the world's impact marks and tracers (UWorld::ImpactMarks / Tracers; Leon's effects, UE's decals and beam
 * emitters): world-space quads built on the CPU into one dynamic vertex buffer per draw, with the world_effects shader
 * and a soft round mask texture.
 *
 * - Impact marks lie on their surface, lifted 0.1 cm along its normal and drawn with a polygon offset, so they never
 *   z-fight with it; each is turned by its serial (the golden angle) so neighbours differ. They multiply the scene's
 *   colour (a dark spot), with the depth test and no depth writes, after the opaque geometry.
 * - Tracers are ribbons facing the camera, added to the scene's colour after the translucent geometry.
 *
 * An empty pool or batch draws nothing and changes no GL state.
 */
class FWorldEffectsRenderer
{
public:
	FWorldEffectsRenderer();
	~FWorldEffectsRenderer();

	FWorldEffectsRenderer(const FWorldEffectsRenderer&) = delete;
	FWorldEffectsRenderer& operator=(const FWorldEffectsRenderer&) = delete;

	/** Loads the shader from ShaderDirectory and makes the buffers and the mask; false on a failure. */
	bool Initialize(const FString& ShaderDirectory);
	void Shutdown();
	/** Reloads the shader from disk if its timestamps changed (or when forced). */
	[[nodiscard]] EShaderReloadResult ReloadShader(bool bForce = false);

	/** Draws the active marks with a world to GL clip space view-projection. */
	void DrawImpactMarks(const FImpactMarkPool& Marks, const FMatrix& ViewProjection);
	/** Draws the tracers as ribbons facing CameraLocation. */
	void DrawTracers(const FTracerBatch& Tracers, const FMatrix& ViewProjection, const FVector& CameraLocation);

	[[nodiscard]] bool IsValid() const
	{
		return Shader.Valid() && Vao != 0 && Mask != nullptr;
	}

	/** The vertex the effects are made of (position, the mask's UV, the colour with the strength in A). */
	struct FEffectVertex
	{
		FVector Position;
		FVector2D TexCoord;
		FLinearColor Color;
	};

	/**
	 * The two triangles of each active mark, in slot order (six vertices a mark; exposed for the tests, it needs no
	 * GPU).
	 */
	static void BuildImpactMarkVertices(const FImpactMarkPool& Marks, TArray<FEffectVertex>& OutVertices);
	/** The two triangles of each tracer, facing CameraLocation. */
	static void BuildTracerVertices(
		const FTracerBatch& Tracers, const FVector& CameraLocation, TArray<FEffectVertex>& OutVertices);

private:
	void Upload(const TArray<FEffectVertex>& Vertices) const;
	void Draw(int32 NumVertices, int32 Mode, const FMatrix& ViewProjection) const;

	FShader Shader;
	TUniquePtr<FTexture2DResource> Mask;
	uint32 Vao = 0;
	uint32 Vbo = 0;
	TArray<FEffectVertex> Vertices;
};
