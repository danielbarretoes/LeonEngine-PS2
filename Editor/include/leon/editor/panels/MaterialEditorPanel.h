#pragma once

#include <leon/editor/EditorContext.h>
#include <leon/render/LeonMaterialFormat.h>
#include <string>
#include <vector>

namespace leon::editor {

/// Dockable Material Editor — each open `.lmat` is its own ImGui window (tabs when docked).
class MaterialEditorPanel {
public:
    void Draw(EditorContext& ctx);
    void OpenMaterial(const std::string& path);

    /// Unreal-like package dirty query / save (File → Save / Save All, Content Browser).
    [[nodiscard]] bool HasDirtyDocs() const;
    [[nodiscard]] bool IsPathDirty(const std::string& path) const;
    [[nodiscard]] bool HasFocusedDirtyDoc() const;
    /// Save the Material Editor window that currently has focus (if dirty).
    [[nodiscard]] bool SaveFocused(EditorContext& ctx);
    [[nodiscard]] bool SavePath(EditorContext& ctx, const std::string& path);
    [[nodiscard]] bool SaveAll(EditorContext& ctx);
    /// Publish open dirty `.lmat` paths into `ctx.dirtyMaterialPaths` for Content Browser * markers.
    void SyncDirtyPaths(EditorContext& ctx) const;
    /// After Content Browser Rename/Move/Delete: update open doc paths (empty `toAbs` closes).
    void RemapAssetPath(const std::string& fromAbs, const std::string& toAbs);

private:
    struct Doc {
        std::string path;
        leon::LeonMaterialDocument data;
        bool dirty = false;
        bool open = true;
        bool focusOnce = false;
        char nameBuf[128]{};
        char baseMapBuf[260]{};
        char normalMapBuf[260]{};
    };

    std::vector<Doc> docs_;
    std::string focusedPath_;
    [[nodiscard]] Doc* FindDoc(const std::string& path);
    [[nodiscard]] const Doc* FindDoc(const std::string& path) const;
    void DrawDoc(EditorContext& ctx, Doc& doc);
    bool SaveDoc(EditorContext& ctx, Doc& doc);
    void ApplyToSelection(EditorContext& ctx, Doc& doc);
};

} // namespace leon::editor
