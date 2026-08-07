#pragma once

#include <leon/editor/EditorContext.h>

namespace leon::editor {

/// World Outliner: Level StaticMeshes / Lights / PlayerStarts / volumes tree + Delete key.
class OutlinerPanel {
public:
    void Draw(EditorContext& ctx);

private:
    void HandleDelete(EditorContext& ctx);
};

} // namespace leon::editor
