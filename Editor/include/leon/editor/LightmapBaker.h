#pragma once

#include <leon/level/Level.h>
#include <string>

namespace leon::editor {

/// Edit-time Build Lights (Unreal-like): bake diffuse lightmaps for Static-mobility meshes.
/// Returns number of meshes baked. Optional message for UI / log.
/// When `levelPath` is non-empty, writes `Lightmaps/LM_<lightmapId>.lm` beside the `.llev` and
/// sets `lightmapPath` / `lightmapId` (stable across actor reorder).
/// Shipping loads `.lm` via Engine `LightmapIO` / `LoadLevelLightmaps` — not this API.
[[nodiscard]] int BakeLevelLightmaps(Level& level, std::string* outMessage = nullptr,
                                     const std::string& levelPath = "");

} // namespace leon::editor
