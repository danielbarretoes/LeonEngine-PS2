#pragma once

#include "CoreTypes.h"

#include <string>

/**
 * DCC source -> cooked `.lmesh` (edit time / LeonCook); shipping loads `.lmesh` through LeonMeshFormat
 * (UE: FStaticMeshBuilder).
 */
struct MESHUTILITIES_API FStaticMeshBuilder
{
	[[nodiscard]] static bool CookFromObj(const std::string& ObjPath, const std::string& OutMeshPath, std::string& OutError);

	[[nodiscard]] static bool CookFromFbx(const std::string& FbxPath, const std::string& OutMeshPath, std::string& OutError);

	[[nodiscard]] static bool CookFromGltf(const std::string& GltfPath, const std::string& OutMeshPath,
		const std::string& MaterialsOutDir, std::string& OutError);
};
