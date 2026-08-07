#pragma once

#include <cstdint>
#include <leon/editor/EditorContext.h>
#include <leon/editor/MaterialSpherePreview.h>
#include <string>
#include <vector>

namespace leon::editor {

/// Content Browser: hierarchy tree or flat contents grid with path navigation.
class ContentBrowserPanel {
public:
    enum class EViewMode : std::uint8_t {
        Hierarchy = 0,
        Contents = 1,
    };

    struct Entry {
        std::string name;
        std::string path;
        bool isDirectory = false;
        enum class Kind {
            Folder,
            Level,
            Material,
            StaticMesh,
            FbxSource,
            Character,
            SkelMesh,
            Skeleton,
            Anim,
            Hdr,
            EnginePrimitive,
            Other
        } kind = Kind::Other;
        std::vector<Entry> children;
        /// Optional albedo swatch for materials (fallback before 3D thumb is ready).
        float swatch[3] = {0.35f, 0.35f, 0.38f};
        bool hasSwatch = false;
    };

    void Draw(EditorContext& ctx);
    void Refresh(EditorContext& ctx);

private:
    void SyncRoot(EditorContext& ctx);
    [[nodiscard]] static Entry::Kind ClassifyFile(const std::string& path);
    [[nodiscard]] Entry BuildTree(const std::string& absoluteDir) const;
    void DrawEntry(EditorContext& ctx, const Entry& entry);
    void DrawEngineLibrary(EditorContext& ctx);
    void DrawBreadcrumbs(EditorContext& ctx);
    void DrawContentsGrid(EditorContext& ctx);
    void DrawCreateContextMenu(EditorContext& ctx);
    void DrawNewMaterialModal(EditorContext& ctx);
    void DrawNewFolderModal(EditorContext& ctx);
    void DrawRenameAssetModal(EditorContext& ctx);
    void DrawDeleteAssetsModal(EditorContext& ctx);
    void DrawAssetItemContextMenu(EditorContext& ctx, const Entry& entry);
    void AcceptFolderDropTarget(EditorContext& ctx, const Entry& folderEntry);
    void ActivateEntry(EditorContext& ctx, const Entry& entry, bool openFolder);
    void BeginRenameAsset(EditorContext& ctx, const std::string& path);
    void BeginDeleteAsset(EditorContext& ctx, const std::string& path);
    [[nodiscard]] const Entry* FindEntryByPath(const Entry& node, const std::string& path) const;
    [[nodiscard]] std::vector<Entry> CurrentFolderChildren() const;
    void EnsureMaterialSwatch(Entry& entry) const;

    std::string contentRoot_;
    std::string lastLevelPath_;
    std::string currentFolder_;
    Entry root_;
    bool scanned_ = false;
    EViewMode viewMode_ = EViewMode::Contents;
    MaterialThumbnailCache materialThumbs_;
    MeshThumbnailCache meshThumbs_;
    bool openNewMaterialModal_ = false;
    bool openNewFolderModal_ = false;
    bool openRenameModal_ = false;
    bool openDeleteModal_ = false;
    std::string modalAssetPath_;
    int deleteRefCount_ = 0;
    char renameNameBuf_[128]{};
};

} // namespace leon::editor
