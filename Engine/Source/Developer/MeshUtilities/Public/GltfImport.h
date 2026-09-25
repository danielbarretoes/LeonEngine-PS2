#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"

struct MESHUTILITIES_API FGltfImportedMaterial
{
	FString Name;
	FString LmatRelativePath; // path written relative to the output directory
};

/**
 * Loads the meshes of a .gltf / .glb (all primitives merged) into FMeshData, in the engine world. When MaterialsOutDir
 * is not empty it also writes a .lmat per material (and copies the textures) there. Edit time / cook only.
 */
[[nodiscard]] MESHUTILITIES_API bool LoadStaticMeshFromGltf(const FString& Path, FMeshData& Out,
	const FString& MaterialsOutDir, TArray<FGltfImportedMaterial>* OutMaterials, FString& OutError);
