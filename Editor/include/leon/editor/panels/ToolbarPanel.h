#pragma once

#include <leon/editor/EditorContext.h>

namespace leon::editor {

/// Top toolbar: gizmo mode (W/E/R), Local/World, and Unreal-like Play / Pause / Stop.
class ToolbarPanel {
public:
    void Draw(EditorContext& ctx);
    /// Shared Play/Pause/Stop controls (also used from the main menu bar).
    static void DrawPlayControls(EditorContext& ctx);
};

} // namespace leon::editor
