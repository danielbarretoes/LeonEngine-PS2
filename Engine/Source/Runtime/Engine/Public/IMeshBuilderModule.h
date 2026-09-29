#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FLPS2Mesh;
struct FMeshData;
struct FSkinWeightInfo;

/**
 * Builds a mesh's render data from its source (UE: IMeshBuilderModule, which UStaticMesh's build calls). The
 * edit-time module MeshUtilities implements it with meshoptimizer (FLPS2MeshBuilder); games and the PS2 do not link
 * it, since they load built meshes, so a mesh can only be built where it is (LeonCook, LeonAutomationTests).
 */
class ENGINE_API IMeshBuilderModule : public IModuleInterface
{
public:
	/** The module that builds meshes, or null when this program does not link it (UE: GetForRunningPlatform). */
	[[nodiscard]] static IMeshBuilderModule* GetForRunningPlatform();

	/** Builds the LPS2 v2 render data of Source; false with OutError when it cannot. */
	virtual bool BuildMesh(const FMeshData& Source, FLPS2Mesh& OutRenderData, FString& OutError) = 0;

	/**
	 * A LOD of Source (Docs/PLANS/ps2-shipping.md N15; UE: IMeshReduction): each section simplified to about
	 * PercentTriangles of its triangles by meshoptimizer (meshopt_simplify, which keeps the attribute seams and the
	 * borders), the vertices as they are. The same source gives the same result. False with OutError when it cannot.
	 */
	virtual bool SimplifyMesh(
		const FMeshData& Source, float PercentTriangles, FMeshData& OutSimplified, FString& OutError) = 0;

	/** Builds a skinned mesh's LPS2 v2 render data (palettes and skin: one FSkinWeightInfo a vertex of Source). */
	virtual bool BuildSkinnedMesh(const FMeshData& Source, const TArray<FSkinWeightInfo>& SkinWeights,
		FLPS2Mesh& OutRenderData, FString& OutError) = 0;
};
