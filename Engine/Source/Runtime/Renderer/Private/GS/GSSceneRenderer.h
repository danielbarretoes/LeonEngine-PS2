#pragma once

#include "CanvasTypes.h"
#include "CoreMinimal.h"
#include "GSCommandList.h"
#include "GSDrawEnvironment.h"
#include "GSPrimitiveEmitter.h"
#include "GSTextureCache.h"
#include "GSVertexBatch.h"
#include "RendererInterface.h"
#include "WorldEffectsGeometry.h"

class FCanvas;
class FDebugDraw;
struct FLPS2ColorStreams;
class FLPS2Mesh;
class FPrimitiveSceneProxy;
class FSceneView;
class FSceneViewFamily;
class FStaticMeshSceneProxy;
class FSkeletalMeshSceneProxy;
struct FLPS2Batch;
struct FLPS2ColorStreams;
struct FMaterial;
struct FPrimitiveSceneInfo;

/**
 * The scene renderer of the GS (Docs/PLANS/ps2-gs-parity.md P5, Docs/PLANS/ps2-engine.md E2): a view family of the
 * scene becomes an FGSCommandList recorded against a drawing environment, which the PS2 sends to the GIF and the
 * desktop executes on its OpenGL emulation of the GS (P4). Everything the GS cannot do per pixel happens here per
 * vertex, in C++ that runs on the EE and on the PC alike (D2):
 *
 * - The transform (UE's view and projection), clipping and back face culling (FGSPrimitiveEmitter), skinning on the
 *   CPU with the pose's skin matrices.
 * - A static or skinned mesh is drawn from its LPS2 v2 render data (FLPS2Mesh), batch by batch (plan D1, D8): a
 *   batch whose sphere is outside the view is skipped, one inside the guard band and the near and far planes is a
 *   vertex batch (FGSVertexBatch), and any other goes through the clipper triangle by triangle. With SetVertexBatches
 *   a static vertex batch is recorded as the list's command, which the PS2 draws on VU1 (ps2-shipping N14), when a
 *   microprogram does its lighting (baked or unlit, or the ambient, one sun and up to two point lights); otherwise
 *   FGSPrimitiveEmitter::AddVertexBatch sends its triangle strips (TRISTRIP: the vertices of the drawn triangles, XYZ3
 *   for those that close none), the reference of VU1's packet. A skinned batch is placed by the sphere its pose
 *   keeps it in (MakeSkinPalette), without skinning a vertex; recorded, VU1 skins it (N14b), else the emitter does.
 * - Static lighting (Docs/PLANS/ps2-shipping.md N22): a Static component's lit sections draw with the vertex colours
 *   LeonEd baked for the instance (the lights with their shadows, and the sky with its occlusion), with no light
 *   computed per frame; without a bake, with the mesh's own colours.
 * - Dynamic lighting of what moves (Movable components, skinned meshes): Lambert per vertex, the map's environment
 *   light (its world settings' LightmassSettings) as ambient, then up to MaxDirectionalLights directional and
 *   MaxPointLights point lights (range attenuation squared), unshadowed. There is no specular, normal map or
 *   reflection: the GS has no pixel stage.
 * - Materials: the albedo times the light (unlit: the albedo), the albedo map through the texture cache with UvScale,
 *   translucent (Alpha < 1) sections blended back to front without writing Z.
 * - Mipmaps (Docs/PLANS/ps2-shipping.md N13): a texture with levels samples them trilinear (MMIN LINEAR_MIPMAP_LINEAR,
 *   MXL its last level) with the GS's LOD from Q (LCM 0): LOD = log2(1/Q) + K with L = 0, where 1/Q is the vertex's
 *   w (its depth, cm) and K = log2(Texels / Pixels) + the material's LodBias. Texels is the section's texels per
 *   centimetre: the texture's sqrt(width x height) times its UV density (FLPS2Mesh::GetSectionUvDensity, the UvScale's
 *   sqrt(X x Y), over the mesh's largest axis scale); Pixels is the frame's pixels per centimetre at a depth of 1 cm,
 *   sqrt((P[0][0] x Width / 2) x (P[1][1] x Height / 2)) of the pass's projection. So a texel is a pixel where the
 *   LOD is 0, and each doubling of the distance goes one level down. A material without bMipmaps, or a section
 *   without UV area, samples level 0 bilinear.
 * - Opaque sections draw grouped by texture, the groups in the order their first section comes in the scene, and a
 *   texture's TEX0, MIPTBP1 / MIPTBP2 and CLAMP are written only when they change, so a frame loads each CLUT once.
 *
 * - The scene (Docs/PLANS/ps2-shipping.md N15): the primitives of the cells the view sees through the map's portals
 *   (FScene::GetVisibilityCells; all of them without cells), each culled by its bounds against the view's frustum
 *   (FFrustum, VU0 on the PS2; a skinned mesh by its pose's bounds, the view model pass by its own projection's), a
 *   static mesh at the LOD of its screen size (ComputeStaticMeshLOD, SetLODDistanceScale); the frame's lists and the
 *   emitter's per-batch vertices on the scratchpad (FScratchpad, FSceneRenderList).
 * - Fog (N15): the world settings' linear distance fog (FWorldFogSettings), the GS's per-vertex fog with FOGCOL, on
 *   the world pass's meshes, blob shadows, impact marks and effect sprites.
 *
 * The frame: the clear, the opaque meshes, the skinned meshes, the blob shadows under what casts them, the impact marks
 * (a lerp toward the mark's colour: the GS cannot multiply by the destination), the translucent meshes, the effect
 * sprites, the tracers (added), the world's debug lines and the show flags' (F1 boxes, F6 axes), then the view model
 * meshes, static and skinned (first-person arms), over a cleared Z buffer with their own projection. The canvas (HUD,
 * text) draws after, with DrawCanvas: its rectangles as SPRITEs.
 *
 * The skinned meshes it draws get the world's time as their LastRenderTime, and the view's location goes to the world's
 * ViewLocationsRenderedLastFrame: the skeletal meshes throttle their poses by both (N25).
 */
class FGSSceneRenderer
{
public:
	/** The background. */
	static const FLinearColor ClearColor;
	/** How much nearer, in PSMZ24 units, the impact marks are drawn than the surface under them. */
	static constexpr uint32 DecalDepthBias = 256;

	[[nodiscard]] FGSTextureCache& GetTextureCache()
	{
		return TextureCache;
	}

	/** Records the family's first view into List. */
	void Render(const FSceneViewFamily& ViewFamily, const FGSDrawEnvironment& Environment, FGSCommandList& List);

	/**
	 * Records the canvas into List, blended, without the depth test: its rectangles (tiles, glyphs, lines along an
	 * axis) as SPRITEs of two vertices, its rotated tiles and slanted lines as triangles (FCanvas::GetPrimitives, N15);
	 * the textured ones sample their texture by UV through the texture cache (the font's pages, UImage's brushes).
	 */
	void DrawCanvas(const FCanvas& Canvas, const FGSDrawEnvironment& Environment, FGSCommandList& List);

	/** Scales the distance static meshes' LODs are chosen by (FRendererSettings::StaticMeshLODDistanceScale). */
	void SetLODDistanceScale(float InLODDistanceScale)
	{
		LODDistanceScale = InLODDistanceScale;
	}

	/**
	 * Forgets the texture of an asset whose data changed or is going away. True when its texture was resident: lists
	 * not sent yet may hold its data in place (FGSTextureCache::Release).
	 */
	bool ReleaseAssetResources(const UObject* Asset)
	{
		const bool bResident = TextureCache.Release(Asset);
		return bResident;
	}

	[[nodiscard]] const FFrameStats& GetFrameStats() const
	{
		return FrameStats;
	}

	/**
	 * Whether the batches inside the guard band are recorded as the list's vertex batch commands (the PS2 with
	 * VU1) or sent as GS writes by the C++ emitter (the desktop, the tests, the PS2's -novu1). Off by default.
	 */
	void SetVertexBatches(bool bInVertexBatches)
	{
		bVertexBatches = bInVertexBatches;
	}
	[[nodiscard]] bool HasVertexBatches() const
	{
		return bVertexBatches;
	}

private:
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

	/**
	 * Binds the material's texture (TEX0, MIPTBP1 / MIPTBP2, CLAMP, TEX1, each written when it changes) and fills its
	 * state. UvPerCm is the section's UV density over the mesh's scale: texture repeats per world centimetre before
	 * the material's UvScale (0: unknown, level 0 only). The texture falls back to a flat colour when it is not
	 * resident this frame, and to none when it cannot be sampled.
	 */
	FSectionState BindMaterial(const FMaterial& Material, float UvPerCm, FGSCommandList& List);

	/** Writes TEX1 and CLAMP when they differ from what the list holds. */
	void SetSampler(FGSCommandList& List, const FGSTex1& Tex1, const FGSClamp& Clamp);

	/** Draws a skinned mesh's sections through ViewProjection (DrawMeshSection with its pose's skin matrices). */
	void DrawSkeletalMesh(FGSPrimitiveEmitter& Emitter, const FSkeletalMeshSceneProxy& Skeletal,
		const FMatrix& ViewProjection, FGSCommandList& List, const FGSDrawEnvironment& Environment);

	/** Draws a static mesh's section (DrawMeshSection with the proxy's mesh at LODIndex, transform and material). */
	void DrawStaticSection(FGSPrimitiveEmitter& Emitter, const FStaticMeshSceneProxy& Proxy, int32 SectionIndex,
		int32 LODIndex, const FMatrix& ViewProjection, FGSCommandList& List, const FGSDrawEnvironment& Environment);

	/**
	 * The LOD a static mesh draws at in View (ComputeStaticMeshLOD with LODDistanceScale; N15). A Static component with
	 * baked colours stays at LOD 0: its colours are baked for LOD 0's batches.
	 */
	[[nodiscard]] int32 GetStaticMeshLOD(
		const FStaticMeshSceneProxy& Proxy, const FBox& Bounds, const FSceneView& View) const;

	/**
	 * The blob shadows of the drawn primitives that cast one (UPrimitiveComponent::bCastBlobShadow, N15): a soft dark
	 * square on the floor under each (FWorldEffectsGeometry::PlaceBlobShadow), alpha blended with the effects' mask,
	 * nearer than the floor by DecalDepthBias, not writing Z.
	 */
	void DrawBlobShadows(TArrayView<const FPrimitiveSceneProxy* const> Casters, const FMatrix& ViewProjection,
		FGSPrimitiveEmitter& Emitter, FGSCommandList& List, const FGSDrawEnvironment& Environment);

	/**
	 * Draws a section of LPS2 v2 render data (opaque or translucent, as its material says) through ViewProjection: its
	 * batches, each skipped, as strips or through the clipper by its sphere against the view (plan D8). A skinned blob
	 * with SkinMatrices (one a bone; the bind pose where one is missing) draws each batch with its palette's skin
	 * matrices (MakeSkinPalette), placed by the sphere of its pose; recorded, a skinned batch's palette goes into the
	 * list's memory and VU1's Skinned programs pose its vertices. With bStaticLighting
	 * (a Static component) a lit section takes BakedColors (null: the mesh's own colours) and no per frame light (N22);
	 * otherwise it is lit by the frame's point lights that reach WorldBounds (N29), with the ambient and the sun.
	 */
	void DrawMeshSection(FGSPrimitiveEmitter& Emitter, const FLPS2Mesh& Mesh, int32 SectionIndex,
		const FMatrix& LocalToWorld, const FBox& WorldBounds, const FMaterial& Material, bool bStaticLighting,
		const FLPS2ColorStreams* BakedColors, const TArray<FMatrix>* SkinMatrices, const FMatrix& ViewProjection,
		FGSCommandList& List, const FGSDrawEnvironment& Environment);

	/**
	 * A skinned batch's palette, its bones' skin matrices of the pose (the identity where SkinMatrices has none: the
	 * bind pose), into BatchPalette; and the sphere its posed vertices are in, without skinning them: a vertex within
	 * the batch's bind-pose sphere (radius r around c) is posed by a blend of two palette bones, and each bone moves
	 * it within r (times the bone's scale) of where it moves c, so the sphere around those balls holds the batch.
	 */
	void MakeSkinPalette(const FLPS2Mesh& Mesh, const FLPS2Batch& Batch, const TArray<FMatrix>& SkinMatrices,
		FVector& OutCenter, float& OutRadius);

	/** The world's impact marks (a lerp toward their colour, nearer than their surface by DecalDepthBias). */
	void DrawImpactMarks(const FSceneViewFamily& ViewFamily, const FMatrix& ViewProjection,
		FGSPrimitiveEmitter& Emitter, FGSCommandList& List, const FGSDrawEnvironment& Environment);
	/** The world's effect sprites (smoke puffs), square to the view, farthest first, alpha blended with the mask. */
	void DrawEffectSprites(const FSceneViewFamily& ViewFamily, const FSceneView& View, const FMatrix& ViewProjection,
		FGSPrimitiveEmitter& Emitter, FGSCommandList& List, const FGSDrawEnvironment& Environment);
	/** The world's tracers, added, facing the camera (never fogged: the fog's colour would be added). */
	void DrawTracers(const FSceneViewFamily& ViewFamily, const FSceneView& View, const FMatrix& ViewProjection,
		FGSPrimitiveEmitter& Emitter, FGSCommandList& List, const FGSDrawEnvironment& Environment);
	/** Binds the effects' mask (the spot in the alpha); false without a texture arena. */
	bool BindEffectsMask(FGSCommandList& List);
	/** Binds a canvas run's texture (TEX0 when it changes); false when it is not resident this frame. */
	bool BindCanvasTexture(const UTexture2D& Texture, FGSCommandList& List);
	/** Blended triangles of the effects' vertices, not culled. */
	void DrawEffectVertices(FGSPrimitiveEmitter& Emitter, TArrayView<const FWorldEffectVertex> Vertices,
		const FMatrix& ViewProjection, bool bTextured);
	void DrawWorldLines(const FSceneViewFamily& ViewFamily, const FMatrix& ViewProjection, FGSPrimitiveEmitter& Emitter,
		FGSCommandList& List);

	/** The show flags' overlays: F1's world boxes (of WorldMeshes), F6's world axes and view gizmo. */
	void DrawShowFlags(const FSceneViewFamily& ViewFamily, const FSceneView& View, const FMatrix& ViewProjection,
		TArrayView<const FStaticMeshSceneProxy* const> WorldMeshes, FGSPrimitiveEmitter& Emitter, FGSCommandList& List);
	/** Lines of a debug draw through ToClip, depth tested as the environment's test says. */
	static void DrawDebugLines(const FDebugDraw& Lines, const FMatrix& ToClip, FGSPrimitiveEmitter& Emitter);

	/** Sets ZBUF_1 with or without Z writes. */
	static void SetDepthWrite(FGSCommandList& List, const FGSDrawEnvironment& Environment, bool bWrite);

	FGSTextureCache TextureCache;
	FFrameStats FrameStats;
	/** The frame's lights in world space and its ambient light (the world's environment light, RGB). */
	FGSVertexLights FrameLights;
	/** The pass's fog: the world settings' in the world pass, none in the view model pass (N15). */
	FGSVertexFog FrameFog;
	/** FRendererSettings::StaticMeshLODDistanceScale. */
	float LODDistanceScale = 1.0f;
	bool bVertexBatches = false;
	/** The triangles of the frame's recorded vertex batches (VU1 culls some of them). */
	int32 NumBatchTriangles = 0;

	/** The texture state the list holds (context 0), so that an unchanged register is not written again. */
	struct FTextureState
	{
		/** The texture TEX0 samples (an asset, or the key of the renderer's texels); null: unknown. */
		const void* Texture = nullptr;
		FGSTextureBinding Binding;
		bool bHasSampler = false;
		uint64 Tex1 = 0;
		uint64 Clamp = 0;
	};
	FTextureState TextureState;
	/** The frame's pixels per centimetre at a depth of 1 cm through the pass's projection (the LOD's scale). */
	float PixelsPerCm = 1.0f;

	/** An opaque section to draw at a LOD, with its texture's group (the order its texture first came in the scene). */
	struct FOpaqueSection
	{
		const FStaticMeshSceneProxy* Proxy = nullptr;
		int32 Section = 0;
		int32 LOD = 0;
		int32 Group = 0;
	};
	TMap<const UTexture2D*, int32> TextureGroups;

	/** A batch's vertices, transformed and lit, and the triangle each closes (at most 64). */
	TArray<FGSClipVertex> BatchVertices;
	TArray<EGSStripTriangle> BatchTriangles;
	/** A skinned batch's palette (MakeSkinPalette; at most 24 bones). */
	TArray<FGSSkinMatrix> BatchPalette;
	/** The canvas's vertices and runs (DrawCanvas), kept between frames for their capacity. */
	TArray<FCanvasVertex> CanvasVertices;
	TArray<FCanvasPrimitiveRun> CanvasRuns;
	/** The effects' mask texels (BindEffectsMask), built on first use. */
	TArray<uint8> EffectsMaskTexels;
};
