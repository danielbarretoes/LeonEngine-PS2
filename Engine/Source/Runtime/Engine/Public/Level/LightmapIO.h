#pragma once

#include "Engine/Level.h"

#include <filesystem>
#include <memory>
#include <string>

class UTexture2D;

/// Load a raw `.lm` (LM01 + RGBA8) lightmap texture from disk.
[[nodiscard]] std::shared_ptr<UTexture2D> LoadLightmapFile(const std::filesystem::path& Path);

/// Resolve a relative `lightmapPath` against the `.llev` directory (or asset root).
[[nodiscard]] std::filesystem::path ResolveLightmapAbsolutePath(
	const std::string& LevelPath, const std::string& LightmapRel);

/// Load textures for every UStaticMeshComponent that has a non-empty `lightmapPath`.
/// Returns the number of lightmaps successfully loaded.
[[nodiscard]] int LoadLevelLightmaps(ULevel& Level, const std::string& LevelPath, std::string* OutMessage = nullptr);

inline void ReloadLevelLightmaps(ULevel& Level)
{
	(void)LoadLevelLightmaps(Level, {}, nullptr);
}

inline void ReloadLevelLightmapsForPath(ULevel& Level, const std::string& LevelPath)
{
	(void)LoadLevelLightmaps(Level, LevelPath, nullptr);
}

/// Ensure `lightmapId` is a non-empty stable id (generates one if missing). Returns the id.
[[nodiscard]] std::string EnsureLightmapId(UStaticMeshComponent& Mesh);
