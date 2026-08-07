#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace leon::editor {

enum class EEditorLogLevel : std::uint8_t {
    Info = 0,
    Warning = 1,
    Error = 2,
};

struct EditorLogLine {
    EEditorLogLevel level = EEditorLogLevel::Info;
    std::string text;
};

/// Ring-buffer log shared by the Output Log panel (thread-safe append).
class EditorOutputLog {
public:
    static EditorOutputLog& Instance();

    void Append(EEditorLogLevel level, std::string text);
    void Clear();
    [[nodiscard]] std::vector<EditorLogLine> Snapshot() const;
    [[nodiscard]] std::size_t Count() const;

    /// Tee `std::cerr` / `std::cout` into this log (idempotent).
    void InstallStreamTee();

private:
    EditorOutputLog() = default;

    static constexpr std::size_t kMaxLines = 2000;
    mutable std::mutex mutex_;
    std::vector<EditorLogLine> lines_;
    bool teeInstalled_ = false;
};

inline void EditorLogInfo(std::string text) {
    EditorOutputLog::Instance().Append(EEditorLogLevel::Info, std::move(text));
}
inline void EditorLogWarn(std::string text) {
    EditorOutputLog::Instance().Append(EEditorLogLevel::Warning, std::move(text));
}
inline void EditorLogError(std::string text) {
    EditorOutputLog::Instance().Append(EEditorLogLevel::Error, std::move(text));
}

} // namespace leon::editor
