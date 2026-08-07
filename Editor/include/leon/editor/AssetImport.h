#pragma once

#include <string>

namespace leon::editor {

struct EditorContext;

/// Result of a Content Browser / File → Import operation.
struct AssetImportResult {
    bool ok = false;
    std::string message;
    /// Primary asset path to preview after import (.lmesh, .lchar, .lanim, …).
    std::string previewPath;
};

enum class EAssetImportMode : int {
    StaticObj = 0,
    CharacterFbx = 1,
    AnimFbx = 2,
    StaticFbx = 3,
    StaticGltf = 4,
};

struct AssetImportRequest {
    EAssetImportMode mode = EAssetImportMode::StaticObj;
    std::string sourcePath;
    /// Character: optional second FBX for run/locomotion (defaults to sourcePath).
    std::string secondaryFbxPath;
    /// Anim: existing `*.lskel`.
    std::string skeletonJsonPath;
    std::string assetName;
    bool animLooping = true;
};

/// Import into the open project's `Content/assets/` (fallback: ResolveAssetPath("assets")).
[[nodiscard]] bool EditorImportAsset(EditorContext& ctx, const AssetImportRequest& request,
                                     AssetImportResult& out);

} // namespace leon::editor
