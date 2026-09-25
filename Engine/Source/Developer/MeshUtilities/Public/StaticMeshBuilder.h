#pragma once

#include "CoreMinimal.h"

/**
 * DCC source -> cooked .lmesh (edit time / LeonCook); shipping loads .lmesh through LeonMeshFormat
 * (UE: FStaticMeshBuilder).
 */
struct MESHUTILITIES_API FStaticMeshBuilder
{
	[[nodiscard]] static bool CookFromObj(const FString& ObjPath, const FString& OutMeshPath, FString& OutError);

	[[nodiscard]] static bool CookFromFbx(const FString& FbxPath, const FString& OutMeshPath, FString& OutError);

	[[nodiscard]] static bool CookFromGltf(
		const FString& GltfPath, const FString& OutMeshPath, const FString& MaterialsOutDir, FString& OutError);
};
