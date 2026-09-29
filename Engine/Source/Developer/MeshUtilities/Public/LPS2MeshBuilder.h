#pragma once

#include "CoreMinimal.h"
#include "LPS2Mesh.h"
#include "MeshData.h"
#include "SkeletalAnimation.h"

/**
 * Builds a static mesh's LPS2 v2 render data (FLPS2Mesh, Docs/ASSET_FORMATS.md) from its source at edit time, with
 * meshoptimizer (Leon; UE's mesh builder makes its vertex and index buffers). For each section:
 *
 * 1. Quantizes every corner (positions to int16 with the mesh's scale and bias, normals to int8, texture coordinates to
 *    4.12, the colour white until the baked lighting) and welds the corners that became equal.
 * 2. Drops the degenerate triangles, orders the rest for strips (meshopt_optimizeVertexCacheStrip) and makes the strips
 *    (meshopt_stripify with a restart index): each strip starts with two vertices that close no triangle (FlagNoKick,
 *    the GS's ADC), and so does a swap's degenerate triangle.
 * 3. Fills batches of at most FLPS2Mesh::MaxBatchVertices vertices with whole strips; a strip too long for one goes on
 *    in the next batch from its last two vertices again. A batch's texture coordinates stay within +-8 of its whole
 *    offset, so a batch also ends before their span would pass FLPS2Mesh's 4.12 range.
 *
 * The output depends only on the source (no hashing order, time or address): the same source gives the same bytes.
 */
struct MESHUTILITIES_API FLPS2MeshBuilder
{
	/**
	 * Builds OutMesh from Source (one section over every index when it has none); false with OutError for an empty
	 * source or indices out of range.
	 */
	[[nodiscard]] static bool Build(const FMeshData& Source, FLPS2Mesh& OutMesh, FString& OutError);

	/**
	 * A LOD's source (Docs/PLANS/ps2-shipping.md N15): each section of Source simplified by meshopt_simplify to about
	 * PercentTriangles of its triangles (no bound on the error, as UE's MaxDeviation 0), its attribute seams and
	 * borders kept, so it may keep more; the vertices are Source's (Build leaves the unused ones out). A section that
	 * would vanish stays whole. False with OutError for no triangles or a section out of range.
	 */
	[[nodiscard]] static bool Simplify(
		const FMeshData& Source, float PercentTriangles, FMeshData& OutSimplified, FString& OutError);

	/**
	 * Builds a skinned mesh's blob (FLPS2Mesh::FlagSkinned): the same steps, with each corner's two bones and weights
	 * (SkinWeights, one per vertex of Source; the weights add up to 255) welded with it. A batch takes at most
	 * FLPS2Mesh::MaxSkinnedBatchVertices vertices and the bones of at most FLPS2Mesh::MaxPaletteBones (its palette), so
	 * a strip that would bring more starts a new batch; each vertex keeps its bones as palette indices.
	 */
	[[nodiscard]] static bool BuildSkinned(
		const FMeshData& Source, const TArray<FSkinWeightInfo>& SkinWeights, FLPS2Mesh& OutMesh, FString& OutError);
};
