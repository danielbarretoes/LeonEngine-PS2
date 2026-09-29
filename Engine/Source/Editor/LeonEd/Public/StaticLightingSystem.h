#pragma once

#include "CoreMinimal.h"

class UStaticMeshComponent;
class UWorld;

/** What a static lighting bake did (FStaticLightingSystem::Build). */
struct LEONED_API FStaticLightingStats
{
	/** Static mesh components that received baked colours. */
	int32 NumMeshes = 0;
	/** Their LPS2 v2 vertices (a vertex two strips or batches share is baked in each). */
	int32 NumVertices = 0;
	/** Triangles that occlude (the static geometry's, in the bake's tree). */
	int32 NumOccluders = 0;
	/** Lights baked in. */
	int32 NumLights = 0;
	/** Rays cast (occlusion and shadow). */
	int64 NumRays = 0;
};

/**
 * LeonEd's static lighting (UE: FStaticLightingSystem and Lightmass; Docs/PLANS/ps2-shipping.md N22, decision D5: the
 * light is baked by the engine's tools, not by Blender, so it is deterministic). For every vertex of every Static mesh
 * component of a world's level it adds, in linear RGB:
 *
 * - the sky: the world settings' environment light (FLightmassWorldInfoSettings) times the share of the vertex's
 *   hemisphere, cosine-weighted, that sees no geometry within MaxOcclusionDistance: NumOcclusionRays fixed directions
 *   (a stratified set from a fixed seed, the same for every vertex and every bake), turned into the vertex's frame;
 * - each light that is not Movable, as the renderer lights what moves (Lambert; a point light's range attenuation
 *   squared), unless a ray from the vertex toward it meets the static geometry first (the light's CastShadows).
 *
 * The occluders are the triangles of the Static, visible, shadow-casting mesh components (their collision triangles,
 * the source's at full precision) in a bounding volume tree (PhysicsCore's FAabbTree). The light times the mesh's own
 * colour, clamped to 1, becomes the instance's colour streams (UStaticMeshComponent::BakedVertexColors,
 * FLPS2ColorStreams: RGBA8, the mesh's alpha), which the renderer draws with no light computed per frame. It runs on
 * one thread in a fixed order, so the same world bakes the same bytes on every run.
 */
class LEONED_API FStaticLightingSystem
{
public:
	/** Bakes the static lighting of World's persistent level into its Static mesh components. */
	static FStaticLightingStats Build(UWorld& World);

	/** Whether the bake lights Component: Static, a valid mesh, and drawn (visible, its owner not hidden). */
	[[nodiscard]] static bool ReceivesStaticLighting(const UStaticMeshComponent& Component);
};
