#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"

/**
 * DCC source to static mesh data, in the engine world (edit time: LeonEd's static mesh factory builds a UStaticMesh
 * from it). glTF / GLB through cgltf, the only mesh format (Docs/PLANS/ps2-shipping.md D11); the importer converts the
 * source's axes and units to the engine world last (FImportCoordinateConversion).
 */
struct MESHUTILITIES_API FStaticMeshBuilder
{
	/** True for the extensions BuildFromFile reads: gltf, glb (with or without the dot, any case). */
	[[nodiscard]] static bool IsSupportedExtension(const FString& Extension);

	/** Reads SourcePath by its extension; false with OutError on failure. */
	[[nodiscard]] static bool BuildFromFile(const FString& SourcePath, FMeshData& OutData, FString& OutError);

	[[nodiscard]] static bool BuildFromGltf(const FString& GltfPath, FMeshData& OutData, FString& OutError);
};
