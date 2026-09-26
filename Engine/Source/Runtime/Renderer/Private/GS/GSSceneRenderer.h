#pragma once

#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSDrawEnvironment.h"
#include "GSPrimitiveEmitter.h"
#include "GSTextureCache.h"
#include "RendererInterface.h"
#include "WorldEffectsRenderer.h"

class FCanvas;
class FLightSceneProxy;
class FSceneView;
class FSceneViewFamily;
class FStaticMeshSceneProxy;
class FSkeletalMeshSceneProxy;
struct FMaterial;

/**
 * The scene renderer of the GS (Docs/PLANS/ps2-gs-parity.md P5, Docs/PLANS/ps2-engine.md E2): a view family of the
 * scene becomes an FGSCommandList recorded against a drawing environment, which the PS2 sends to the GIF (and the
 * desktop preview will emulate, P4). Everything the GS cannot do per pixel happens here per vertex, in C++ that runs on
 * the EE and on the PC alike (D2):
 *
 * - The transform (UE's view and projection), clipping and back face culling (FGSPrimitiveEmitter), skinning on the
 *   CPU with the pose's skin matrices.
 * - Lambert lighting per vertex, the desktop's diffuse terms: 0.10 of the albedo as ambient, then up to
 *   MaxDirectionalLights directional and MaxPointLights point lights (range attenuation squared). No specular, sky
 *   reflection, normal maps, shadows or planar mirror: the GS has no pixel stage.
 * - Materials: the albedo times the light (unlit: the albedo), the albedo map through the texture cache with UvScale,
 *   translucent (Alpha < 1) sections blended back to front without writing Z.
 *
 * The frame: the clear, the opaque meshes, the skinned meshes, the impact marks (a lerp toward the mark's colour: the
 * GS cannot multiply by the destination as the desktop does), the translucent meshes, the tracers (added), the world's
 * debug lines, then the view model meshes over a cleared Z buffer with their own projection. The canvas (HUD, text)
 * draws after, with DrawCanvas.
 */
class FGSSceneRenderer
{
public:
	/** The background (the desktop renderer's clear colour). */
	static const FLinearColor ClearColor;
	/** How much nearer, in PSMZ24 units, the impact marks are drawn than the surface under them. */
	static constexpr uint32 DecalDepthBias = 256;

	[[nodiscard]] FGSTextureCache& GetTextureCache()
	{
		return TextureCache;
	}

	/** Records the family's first view into List. */
	void Render(const FSceneViewFamily& ViewFamily, const FGSDrawEnvironment& Environment, FGSCommandList& List);

	/** Records the canvas's triangles into List: blended, without the depth test. */
	void DrawCanvas(const FCanvas& Canvas, const FGSDrawEnvironment& Environment, FGSCommandList& List);

	/** Forgets the texture of an asset whose data changed or is going away. */
	void ReleaseAssetResources(const UObject* Asset)
	{
		TextureCache.Release(Asset);
	}

	[[nodiscard]] const FFrameStats& GetFrameStats() const
	{
		return FrameStats;
	}

private:
	/** The lights of a frame, in world space. */
	struct FFrameLights
	{
		TArray<const FLightSceneProxy*> Directional;
		TArray<const FLightSceneProxy*> Point;
	};

	/** A material section's GS state: its colour and alpha, its texture. */
	struct FSectionState
	{
		bool bLit = true;
		bool bTranslucent = false;
		bool bTextured = false;
		FLinearColor Albedo = FLinearColor::White;
		float Alpha = 1.0f;
		FVector2D UvScale = FVector2D(1.0f, 1.0f);
	};

	/** The light reaching a world point with a unit normal (RGB). */
	[[nodiscard]] FVector Irradiance(const FVector& Position, const FVector& Normal) const;

	/** Binds the material's texture (TEX0, TEX1) and fills its state; the texture falls back to none. */
	FSectionState BindMaterial(const FMaterial& Material, FGSCommandList& List);

	/**
	 * Transforms a static mesh's vertices to clip space and lights them (world normals), into ClipPositions and
	 * Irradiances.
	 */
	void TransformStaticMesh(const FStaticMeshSceneProxy& Proxy, const FMatrix& ViewProjection);
	void TransformSkeletalMesh(const FSkeletalMeshSceneProxy& Proxy, const FMatrix& ViewProjection);

	/** Draws the triangles of Indices[First, First + Count) of the transformed mesh with a section's state. */
	void DrawTriangles(FGSPrimitiveEmitter& Emitter, const TArray<uint32>& Indices, int32 First, int32 Count,
		const FSectionState& State, const TArray<FVector2D>& TexCoords);
	/** Draws a static mesh's section (opaque or translucent, as its material says). */
	void DrawStaticSection(FGSPrimitiveEmitter& Emitter, const FStaticMeshSceneProxy& Proxy, int32 SectionIndex,
		FGSCommandList& List, const FGSDrawEnvironment& Environment);

	/** The world's impact marks (a lerp toward their colour, nearer than their surface by DecalDepthBias). */
	void DrawImpactMarks(const FSceneViewFamily& ViewFamily, const FMatrix& ViewProjection,
		FGSPrimitiveEmitter& Emitter, FGSCommandList& List, const FGSDrawEnvironment& Environment);
	/** The world's tracers, added, facing the camera. */
	void DrawTracers(const FSceneViewFamily& ViewFamily, const FSceneView& View, const FMatrix& ViewProjection,
		FGSPrimitiveEmitter& Emitter, FGSCommandList& List, const FGSDrawEnvironment& Environment);
	/** Binds the effects' mask (the spot in the alpha); false without a texture arena. */
	bool BindEffectsMask(FGSCommandList& List);
	/** Blended triangles of the effects' vertices, not culled. */
	void DrawEffectVertices(FGSPrimitiveEmitter& Emitter, const TArray<FWorldEffectsRenderer::FEffectVertex>& Vertices,
		const FMatrix& ViewProjection, bool bTextured);
	void DrawWorldLines(const FSceneViewFamily& ViewFamily, const FMatrix& ViewProjection, FGSPrimitiveEmitter& Emitter,
		FGSCommandList& List);

	/** Sets ZBUF_1 with or without Z writes. */
	static void SetDepthWrite(FGSCommandList& List, const FGSDrawEnvironment& Environment, bool bWrite);

	FGSTextureCache TextureCache;
	FFrameStats FrameStats;
	FFrameLights Lights;

	/** The mesh being drawn: its vertices in clip space, their light and texture coordinates. */
	TArray<FVector4> ClipPositions;
	TArray<FVector> Irradiances;
	TArray<FVector2D> MeshTexCoords;
	TArray<const FStaticMeshSceneProxy*> WorldMeshes;
	TArray<const FStaticMeshSceneProxy*> ViewModelMeshes;
};
