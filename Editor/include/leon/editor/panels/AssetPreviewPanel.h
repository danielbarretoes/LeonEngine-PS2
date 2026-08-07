#pragma once

#include <cstdint>
#include <leon/animation/SkeletalAnimation.h>
#include <leon/core/Camera.h>
#include <leon/editor/EditorContext.h>
#include <leon/editor/EditorViewportTarget.h>
#include <leon/level/Level.h>
#include <leon/render/Material.h>
#include <leon/render/SkeletalMesh.h>
#include <leon/render/StaticMesh.h>
#include <memory>
#include <string>
#include <vector>

namespace leon::editor {

enum class EAssetPreviewKind : std::uint8_t {
    None,
    StaticMesh,
    SkeletalMesh,
    Character,
    Material,
};

/// Unreal-like mesh / skeletal preview with material override and orbit camera.
class AssetPreviewPanel {
public:
    void Draw(EditorContext& ctx);

private:
    void ReloadIfNeeded(EditorContext& ctx);
    void ClearPreview();
    [[nodiscard]] bool LoadStatic(EditorContext& ctx, const std::string& path);
    [[nodiscard]] bool LoadSkeletalCooked(EditorContext& ctx, const std::string& skelmeshJson);
    [[nodiscard]] bool LoadCharacter(EditorContext& ctx, const std::string& characterAsset);
    [[nodiscard]] bool LoadMaterialOnly(EditorContext& ctx, const std::string& materialJson);
    void RenderPreview(EditorContext& ctx);
    void HandleOrbit(EditorContext& ctx);
    void PlaceStaticInLevel(EditorContext& ctx);

    EditorViewportTarget target_;
    leon::Camera camera_{};
    leon::Level previewLevel_{};
    EAssetPreviewKind kind_ = EAssetPreviewKind::None;
    std::string loadedPath_;
    std::string materialPath_;
    std::shared_ptr<leon::StaticMesh> staticMesh_;
    std::unique_ptr<leon::SkeletalMesh> skeletalMesh_;
    std::vector<glm::mat4> bonePose_;
    leon::AnimSequence previewAnim_{};
    float animTime_ = 0.0f;
    bool playAnim_ = true;
    leon::Material overrideMaterial_{};
    bool useMaterialOverride_ = false;
    bool orbitDragging_ = false;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
    char materialEdit_[512]{};
    std::string status_;
};

} // namespace leon::editor
