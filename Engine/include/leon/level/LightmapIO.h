#pragma once

#include <filesystem>
#include <leon/level/Level.h>
#include <memory>
#include <string>

namespace leon {

class Texture;

/// Load a raw `.lm` (LM01 + RGBA8) lightmap texture from disk.
[[nodiscard]] std::shared_ptr<Texture> LoadLightmapFile(const std::filesystem::path& path);

/// Resolve a relative `lightmapPath` against the `.llev` directory (or asset root).
[[nodiscard]] std::filesystem::path ResolveLightmapAbsolutePath(const std::string& levelPath,
                                                                const std::string& lightmapRel);

/// Load textures for every StaticMeshComponent that has a non-empty `lightmapPath`.
/// Returns the number of lightmaps successfully loaded.
[[nodiscard]] int LoadLevelLightmaps(Level& level, const std::string& levelPath,
                                     std::string* outMessage = nullptr);

inline void ReloadLevelLightmaps(Level& level) {
    (void)LoadLevelLightmaps(level, {}, nullptr);
}

inline void ReloadLevelLightmapsForPath(Level& level, const std::string& levelPath) {
    (void)LoadLevelLightmaps(level, levelPath, nullptr);
}

/// Ensure `lightmapId` is a non-empty stable id (generates one if missing). Returns the id.
[[nodiscard]] std::string EnsureLightmapId(StaticMeshComponent& mesh);

} // namespace leon
