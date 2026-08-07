#include <leon/editor/EditorHistory.h>

#include <iostream>
#include <leon/editor/EditorContext.h>
#include <leon/editor/LevelSaver.h>

namespace leon::editor {

std::string EditorHistory::Snapshot(const EditorContext& ctx) {
    if (ctx.level == nullptr || ctx.camera == nullptr) {
        return {};
    }
    return SerializeLevelSnapshot(*ctx.level, *ctx.camera);
}

bool EditorHistory::ApplySnapshot(Engine& engine, EditorContext& ctx, const std::string& snapshot) {
    if (snapshot.empty()) {
        return false;
    }
    // Reuse the level path so pack-relative materials and lightmaps still resolve.
    const std::string sourcePath = ctx.levelPath.empty() ? std::string("undo/redo") : ctx.levelPath;
    applying_ = true;
    const bool ok = LoadLevelSnapshot(engine, snapshot, sourcePath);
    applying_ = false;
    if (!ok) {
        return false;
    }
    ctx.level = &engine.GetLevel();
    ctx.camera = &engine.GetCamera();
    ctx.ClearSelection();
    ctx.dirty = true;
    ctx.requestContentRefresh = true;
    return true;
}

void EditorHistory::Capture(const EditorContext& ctx) {
    if (applying_) {
        return;
    }
    const std::string snap = Snapshot(ctx);
    if (snap.empty()) {
        return;
    }
    if (!undo_.empty() && undo_.back() == snap) {
        return;
    }
    undo_.push_back(snap);
    if (undo_.size() > kMaxDepth) {
        undo_.erase(undo_.begin());
    }
    redo_.clear();
}

bool EditorHistory::Undo(Engine& engine, EditorContext& ctx) {
    if (undo_.empty()) {
        return false;
    }
    const std::string current = Snapshot(ctx);
    const std::string prev = undo_.back();
    // Apply before mutating stacks so a failed restore leaves history intact.
    if (!ApplySnapshot(engine, ctx, prev)) {
        std::cerr << "EditorHistory: undo apply failed\n";
        return false;
    }
    undo_.pop_back();
    if (!current.empty()) {
        redo_.push_back(current);
    }
    return true;
}

bool EditorHistory::Redo(Engine& engine, EditorContext& ctx) {
    if (redo_.empty()) {
        return false;
    }
    const std::string current = Snapshot(ctx);
    const std::string next = redo_.back();
    if (!ApplySnapshot(engine, ctx, next)) {
        std::cerr << "EditorHistory: redo apply failed\n";
        return false;
    }
    redo_.pop_back();
    if (!current.empty()) {
        undo_.push_back(current);
    }
    return true;
}

void EditorHistory::Clear() {
    undo_.clear();
    redo_.clear();
}

} // namespace leon::editor
