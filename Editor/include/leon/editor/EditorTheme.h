#pragma once

namespace leon::editor {

/// Apply Unreal Editor 5–inspired Dear ImGui colors and sizing (call once after CreateContext).
void ApplyEditorTheme();

/// Load Inter from `assets/fonts/` (call after CreateContext, before first NewFrame).
/// Returns false if Inter was missing and the default font was used instead.
[[nodiscard]] bool LoadEditorFonts();

} // namespace leon::editor
