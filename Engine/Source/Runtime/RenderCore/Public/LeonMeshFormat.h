#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"

/** Cooked static mesh binary (.lmesh). Source of truth for runtime until the .lasset packages (P14). */
[[nodiscard]] bool IsLeonMeshPath(const FString& Path);

/**
 * Loads a .lmesh into the engine world. Version 2 files are already in it (UE: X forward, Y right, Z up, left-handed,
 * centimetres) and load as stored. Version 1 files hold legacy data (metres, Y up, right-handed): positions, normals
 * and tangents are converted with FLegacyCoordinateConversion::ConvertMeshData (Y and Z swap, 1 m = 100 cm, the
 * bitangent sign flips). UVs and the index order stay as stored in both.
 */
[[nodiscard]] bool LoadLeonMeshFile(const FString& Path, FMeshData& Out);

/**
 * Writes a version 2 .lmesh: Data is in the engine world, as the importers (MeshUtilities) produce it, and is written
 * unchanged. The layout is the same as version 1; only the space of the data differs.
 */
[[nodiscard]] bool SaveLeonMeshFile(const FString& Path, const FMeshData& Data);
