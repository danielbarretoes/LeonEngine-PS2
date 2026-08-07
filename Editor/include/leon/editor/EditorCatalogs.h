#pragma once

#include <string>
#include <vector>

namespace leon::editor {

struct EditorMaterialEntry {
    std::string displayName;
    /// Path used for loadMaterial / level authoring (often relative like Materials/M_Default.lmat).
    std::string authoringPath;
    /// Absolute path when available (for preview / Material Editor).
    std::string absolutePath;
};

struct EditorSkyboxEntry {
    std::string displayName;
    /// Path stored on the level / passed to loadEnvMap.
    std::string authoringPath;
    std::string absolutePath;
};

/// Authoring GameMode ids for World Settings: built-ins + `leon.game.json` + level overrides.
[[nodiscard]] std::vector<std::string> ListEditorGameModes(const std::string& projectPath = {});

/// Collect `.lmat` assets from Engine + open project pack.
[[nodiscard]] std::vector<EditorMaterialEntry> CollectEditorMaterials(const std::string& projectPath);

/// Collect HDR skyboxes from Engine + open project pack.
[[nodiscard]] std::vector<EditorSkyboxEntry> CollectEditorSkyboxes(const std::string& projectPath);

} // namespace leon::editor
