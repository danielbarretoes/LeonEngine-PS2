#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"

/**
 * LoadObj without the conversion to the engine world: the OBJ's own right-handed, Y-up metres, the data a version 1
 * old cooked meshes stored. Tests compare the converted import against it.
 */
[[nodiscard]] FMeshData LoadObjSourceSpace(const FString& Path);
