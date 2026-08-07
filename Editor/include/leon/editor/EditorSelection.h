#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace leon::editor {

/// What the Outliner / Viewport currently highlights.
enum class EEditorSelectionKind : std::uint8_t {
    None,
    StaticMesh,
    DirectionalLight,
    PointLight,
    PlayerStart,
    TriggerVolume,
    PainCausingVolume,
    AISpawnPoint,
    Actor,
};

/// Editor selection: `id` is session-stable; `index` is the current array slot (refreshed by
/// `EditorContext::ResolveSelectionIndices`). Prefer matching by `id` when non-zero.
struct EditorSelection {
    EEditorSelectionKind kind = EEditorSelectionKind::None;
    std::size_t index = 0;
    std::uint64_t id = 0;

    [[nodiscard]] bool IsValid() const { return kind != EEditorSelectionKind::None; }

    void Clear() {
        kind = EEditorSelectionKind::None;
        index = 0;
        id = 0;
    }

    [[nodiscard]] bool Equals(EEditorSelectionKind otherKind, std::size_t otherIndex) const {
        return kind == otherKind && index == otherIndex;
    }

    [[nodiscard]] bool EqualsId(EEditorSelectionKind otherKind, std::uint64_t otherId) const {
        return kind == otherKind && id != 0 && id == otherId;
    }

    [[nodiscard]] bool operator==(const EditorSelection& other) const {
        if (id != 0 && other.id != 0) {
            return kind == other.kind && id == other.id;
        }
        return Equals(other.kind, other.index);
    }
};

/// Multi-selection set; `primary` drives Details / gizmo.
struct EditorSelectionSet {
    EditorSelection primary{};
    std::vector<EditorSelection> items;

    [[nodiscard]] bool IsValid() const { return primary.IsValid(); }
    [[nodiscard]] bool Empty() const { return items.empty(); }
    [[nodiscard]] std::size_t Count() const { return items.size(); }

    void Clear() {
        primary.Clear();
        items.clear();
    }

    [[nodiscard]] bool Contains(EEditorSelectionKind kind, std::size_t index) const {
        for (const EditorSelection& s : items) {
            if (s.Equals(kind, index)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool ContainsId(EEditorSelectionKind kind, std::uint64_t id) const {
        if (id == 0) {
            return false;
        }
        for (const EditorSelection& s : items) {
            if (s.EqualsId(kind, id)) {
                return true;
            }
        }
        return false;
    }

    void Set(EEditorSelectionKind kind, std::size_t index, std::uint64_t id = 0) {
        primary.kind = kind;
        primary.index = index;
        primary.id = id;
        items.clear();
        if (primary.IsValid()) {
            items.push_back(primary);
        }
    }

    void Toggle(EEditorSelectionKind kind, std::size_t index, std::uint64_t id = 0) {
        if (!primary.IsValid()) {
            Set(kind, index, id);
            return;
        }
        for (auto it = items.begin(); it != items.end(); ++it) {
            const bool hit = (id != 0 && it->id != 0) ? it->EqualsId(kind, id)
                                                      : it->Equals(kind, index);
            if (hit) {
                items.erase(it);
                if ((id != 0 && primary.id == id) || primary.Equals(kind, index)) {
                    primary = items.empty() ? EditorSelection{} : items.back();
                }
                return;
            }
        }
        EditorSelection s{kind, index, id};
        items.push_back(s);
        primary = s;
    }
};

} // namespace leon::editor
