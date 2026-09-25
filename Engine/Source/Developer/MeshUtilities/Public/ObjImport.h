#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"

/**
 * Wavefront OBJ -> FMeshData (CPU only) in the engine world, without tangents. Edit time / cook; not linked by shipping
 * builds.
 */
[[nodiscard]] MESHUTILITIES_API FMeshData LoadObj(const FString& Path);
