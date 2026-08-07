#pragma once

#include <cstdint>
#include <string>

namespace leon::editor {

enum class EEditorToastKind : std::uint8_t {
    Info = 0,
    Success = 1,
    Warning = 2,
    Error = 3,
};

/// Thread-safe toast queue (build status, short notices). Drawn each frame via DrawEditorToasts().
void EditorToast(std::string message, EEditorToastKind kind = EEditorToastKind::Info,
                 float seconds = 3.5f);

void DrawEditorToasts();

} // namespace leon::editor
