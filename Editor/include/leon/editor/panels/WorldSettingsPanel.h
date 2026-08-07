#pragma once

#include <leon/editor/EditorCatalogs.h>
#include <leon/editor/EditorContext.h>
#include <string>
#include <vector>

namespace leon::editor {

/// Per-level World Settings: name, GameMode Override, environment, post process.
/// Pack-wide options (Game Default Map, etc.) live in Project Settings.
class WorldSettingsPanel {
public:
    void Draw(EditorContext& ctx);

private:
    void RefreshCatalogs(EditorContext& ctx);

    std::vector<EditorSkyboxEntry> skyboxes_;
    std::string catalogsProjectKey_;
};

} // namespace leon::editor
