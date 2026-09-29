#include "IMeshBuilderModule.h"
#include "LPS2MeshBuilder.h"
#include "Modules/ModuleManager.h"

/**
 * The module builds the meshes' render data for UStaticMesh::BuildFromMeshData and USkeletalMesh::BuildFromMeshData
 * (UE: FMeshBuilderModule).
 */
class FMeshUtilitiesModule final : public IMeshBuilderModule
{
public:
	bool BuildMesh(const FMeshData& Source, FLPS2Mesh& OutRenderData, FString& OutError) override
	{
		return FLPS2MeshBuilder::Build(Source, OutRenderData, OutError);
	}

	bool SimplifyMesh(
		const FMeshData& Source, float PercentTriangles, FMeshData& OutSimplified, FString& OutError) override
	{
		return FLPS2MeshBuilder::Simplify(Source, PercentTriangles, OutSimplified, OutError);
	}

	bool BuildSkinnedMesh(const FMeshData& Source, const TArray<FSkinWeightInfo>& SkinWeights, FLPS2Mesh& OutRenderData,
		FString& OutError) override
	{
		return FLPS2MeshBuilder::BuildSkinned(Source, SkinWeights, OutRenderData, OutError);
	}
};

IMPLEMENT_MODULE(FMeshUtilitiesModule, MeshUtilities)
