#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"

/**
 * DCC source to static mesh data, in the engine world with tangents (edit time: LeonEd's static mesh factories build a
 * UStaticMesh from it). OBJ through tinyobjloader, FBX through ufbx, glTF / GLB through cgltf; each importer converts
 * the source's axes and units to the engine world last (FImportCoordinateConversion).
 */
struct MESHUTILITIES_API FStaticMeshBuilder
{
	/** True for the extensions BuildFromFile reads: obj, fbx, gltf, glb (with or without the dot, any case). */
	[[nodiscard]] static bool IsSupportedExtension(const FString& Extension);

	/** Reads SourcePath by its extension; false with OutError on failure. */
	[[nodiscard]] static bool BuildFromFile(const FString& SourcePath, FMeshData& OutData, FString& OutError);

	[[nodiscard]] static bool BuildFromObj(const FString& ObjPath, FMeshData& OutData, FString& OutError);

	[[nodiscard]] static bool BuildFromFbx(const FString& FbxPath, FMeshData& OutData, FString& OutError);

	[[nodiscard]] static bool BuildFromGltf(const FString& GltfPath, FMeshData& OutData, FString& OutError);
};
