#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"

/** Cooked static mesh binary (.lmesh). Source of truth for runtime until the .lasset packages (P14). */
[[nodiscard]] bool IsLeonMeshPath(const FString& Path);

/**
 * Loads a .lmesh into the engine world. Version 1 files hold legacy metres (Y up): positions are converted with
 * FLegacyCoordinateConversion (1 m = 100 cm); normals, tangents and UVs are unitless and stay as stored.
 */
[[nodiscard]] bool LoadLeonMeshFile(const FString& Path, FMeshData& Out);

/**
 * Writes a version 1 .lmesh. Data is in legacy metres, as the importers (MeshUtilities) produce it until they convert
 * to the engine world themselves, so it is written unchanged: cooked output stays byte-identical.
 */
[[nodiscard]] bool SaveLeonMeshFile(const FString& Path, const FMeshData& Data);
