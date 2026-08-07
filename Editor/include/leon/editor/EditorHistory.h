#pragma once

#include <string>
#include <vector>

namespace leon {
class Engine;
} // namespace leon

namespace leon::editor {

struct EditorContext;

/// Snapshot-based undo/redo over `.llev` byte snapshots (Unreal-like transaction stack lite).
class EditorHistory {
public:
    /// Capture current level+camera before a mutating edit.
    void Capture(const EditorContext& ctx);
    [[nodiscard]] bool CanUndo() const { return !undo_.empty(); }
    [[nodiscard]] bool CanRedo() const { return !redo_.empty(); }
    [[nodiscard]] bool Undo(leon::Engine& engine, EditorContext& ctx);
    [[nodiscard]] bool Redo(leon::Engine& engine, EditorContext& ctx);
    void Clear();

private:
    static constexpr std::size_t kMaxDepth = 64;
    std::vector<std::string> undo_;
    std::vector<std::string> redo_;
    bool applying_ = false;

    [[nodiscard]] static std::string Snapshot(const EditorContext& ctx);
    [[nodiscard]] bool ApplySnapshot(leon::Engine& engine, EditorContext& ctx,
                                     const std::string& snapshot);
};

} // namespace leon::editor
