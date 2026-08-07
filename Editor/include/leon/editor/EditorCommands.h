#pragma once

#include <glm/vec3.hpp>
#include <leon/core/Transform.h>
#include <leon/editor/EditorContext.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/EditorSelection.h>
#include <string>
#include <vector>

namespace leon::editor {

/// Clipboard entry for editor copy/paste of level objects.
struct EditorClipboardItem {
    EEditorSelectionKind kind = EEditorSelectionKind::None;
    /// Serialized actor/light JSON object (schema fragment).
    std::string json;
};

/// Shared edit ops: delete, duplicate, copy/paste (works with multi-select).
class EditorCommands {
public:
    static void DeleteSelection(EditorContext& ctx, EditorHistory* history);
    static void DuplicateSelection(EditorContext& ctx, EditorHistory* history);
    static void CopySelection(EditorContext& ctx);
    static void PasteClipboard(EditorContext& ctx, EditorHistory* history);
    [[nodiscard]] static bool HasClipboard() { return !Clipboard().empty(); }

    /// Snap a world position to the editor grid.
    [[nodiscard]] static glm::vec3 SnapPosition(const glm::vec3& p, float gridSize);
    [[nodiscard]] static float SnapAngle(float degrees, float stepDegrees);
    static void SnapTransform(leon::Transform& t, float gridSize, float angleStep, bool snapScale);

private:
    static std::vector<EditorClipboardItem>& Clipboard();
};

} // namespace leon::editor
