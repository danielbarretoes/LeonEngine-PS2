#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"

/**
 * Loads a static (non-skinned) mesh from FBX via ufbx; all mesh nodes are merged with submeshes.
 * Edit time / cook only.
 */
[[nodiscard]] MESHUTILITIES_API bool LoadStaticMeshFromFbx(const FString& Path, FMeshData& Out);
