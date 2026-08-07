#pragma once

#include <leon/editor/EditorCatalogs.h>
#include <leon/editor/EditorContext.h>
#include <leon/editor/MaterialSpherePreview.h>
#include <string>
#include <vector>

namespace leon::editor {

/// Details panel: Transform + gameplay flags for the current selection.
class DetailsPanel {
public:
    void Draw(EditorContext& ctx);

private:
    void DrawMaterialPicker(EditorContext& ctx, StaticMeshComponent& mesh);
    void RefreshMaterialList(EditorContext& ctx);

    MaterialSpherePreview materialPreview_;
    std::vector<EditorMaterialEntry> materials_;
    std::string materialsProjectKey_;
};

} // namespace leon::editor
