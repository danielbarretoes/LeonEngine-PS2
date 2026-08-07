#pragma once

#include <string>

namespace leon::editor {

/// Native open-file dialog (Windows). Returns empty string on cancel.
[[nodiscard]] std::string EditorPickOpenFile(const char* filter, const char* title);

/// Native save-file dialog (Windows). Returns empty string on cancel.
[[nodiscard]] std::string EditorPickSaveFile(const char* filter, const char* title,
                                             const char* defaultName = nullptr);

/// Native folder picker (Windows). Returns empty string on cancel.
[[nodiscard]] std::string EditorPickFolder(const char* title);

} // namespace leon::editor
