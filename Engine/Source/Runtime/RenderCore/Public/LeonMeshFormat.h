#pragma once

#include "MeshData.h"
#include <string>


/// Cooked static mesh binary (`.lmesh`). Source of truth for runtime.
[[nodiscard]] bool IsLeonMeshPath(const std::string& Path);

[[nodiscard]] bool LoadLeonMeshFile(const std::string& Path, FMeshData& Out);
[[nodiscard]] bool SaveLeonMeshFile(const std::string& Path, const FMeshData& Data);

