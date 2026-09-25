#pragma once

#include "CoreMinimal.h"
#include "MeshData.h"

/** Cooked static mesh binary (.lmesh). Source of truth for runtime until the .lasset packages (P14). */
[[nodiscard]] bool IsLeonMeshPath(const FString& Path);

[[nodiscard]] bool LoadLeonMeshFile(const FString& Path, FMeshData& Out);
[[nodiscard]] bool SaveLeonMeshFile(const FString& Path, const FMeshData& Data);
