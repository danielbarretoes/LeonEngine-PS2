#pragma once

#include "MeshData.h"
#include <string>


/// Cooked static mesh binary (`.lmesh`). Source of truth for runtime.
[[nodiscard]] bool IsLeonMeshPath(const std::string& path);

[[nodiscard]] bool LoadLeonMeshFile(const std::string& path, FMeshData& out);
[[nodiscard]] bool SaveLeonMeshFile(const std::string& path, const FMeshData& data);

