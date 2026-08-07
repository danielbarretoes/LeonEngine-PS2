#pragma once

#include <cstdint>
#include <imgui.h>
#include <leon/core/Camera.h>
#include <leon/core/Transform.h>
#include <leon/editor/EditorContext.h>
#include <leon/editor/EditorViewportTarget.h>
#include <leon/editor/TransformGizmo.h>
#include <leon/physics/PhysScene.h>
#include <leon/render/Material.h>
#include <optional>
#include <string>
#include <vector>

namespace leon::editor {

enum class EViewportDrag : std::uint8_t {
    None,
    Pan,
    Orbit,
    Look,
    Dolly,
};

/// Central Viewport: FBO scene + ImGuizmo + Unreal-like camera + picking + selection AABB.
class ViewportPanel {
public:
    void Draw(EditorContext& ctx);
    /// Renders the Level into the FBO (also called from `Draw` before `ImGui::Image`).
    void RenderScene(EditorContext& ctx);

    [[nodiscard]] EditorViewportTarget& Target() { return target_; }

private:
    void HandleCameraInput(EditorContext& ctx, float deltaTime);
    void HandleViewportPicking(EditorContext& ctx);
    void DrawEditorHelpers(EditorContext& ctx);
    void DrawSelectionOverlay(EditorContext& ctx);
    void FocusSelection(EditorContext& ctx);
    void ApplyViewMode(EditorContext& ctx, leon::Camera& camera);
    /// Sync PhysScene from the level and queue Player Collision wireframes.
    void AppendPlayerCollisionOverlay(EditorContext& ctx);
    void DrawViewModeOverlay(EditorContext& ctx, const ImVec2& imageMin);
    [[nodiscard]] leon::Transform* SelectedTransform(EditorContext& ctx);
    [[nodiscard]] bool SelectedFocusPoint(EditorContext& ctx, glm::vec3& outPoint);

    void DrawStatsOverlay(EditorContext& ctx, const ImVec2& imageMin);
    [[nodiscard]] std::optional<std::size_t> PickStaticMeshUnderMouse(EditorContext& ctx) const;
    void ClearMaterialDropPreview(EditorContext& ctx);
    void ApplyMaterialDropPreview(EditorContext& ctx, std::size_t meshIndex,
                                  const std::string& materialPath, const Material& material);
    [[nodiscard]] bool HandleMaterialDrag(EditorContext& ctx, const std::string& path,
                                          bool isDelivery);

    EditorViewportTarget target_;
    TransformGizmo gizmo_;
    leon::Transform actorGizmoScratch_{};
    EViewportDrag dragMode_ = EViewportDrag::None;
    bool flyActive_ = false;
    bool gizmoWasUsing_ = false;
    EEditorViewMode lastAppliedViewMode_ = EEditorViewMode::Perspective;
    /// Edit-mode collision preview when `ctx.world` is null (outside PIE).
    leon::PhysScene collisionPreviewScene_;
    float statsAccumTime_ = 0.0f;
    int statsAccumFrames_ = 0;
    float displayFps_ = 0.0f;
    float displayMs_ = 0.0f;

    /// Temporary material swap while dragging a .lmat onto a mesh under the cursor.
    bool materialPreviewActive_ = false;
    std::size_t materialPreviewMeshIndex_ = 0;
    Material materialPreviewSaved_{};
    std::vector<Material> materialPreviewSavedSlots_{};
    bool materialPreviewSavedOverride_ = false;
    std::string materialPreviewSavedPath_;
    std::string materialPreviewPath_;

    static void EnsureEditorFreeLook(leon::Camera& camera);
    void HandleAssetDrop(EditorContext& ctx);
    void DrawGrid(EditorContext& ctx);
    /// Corner XYZ triad (ImGui overlay) aligned to the active view camera.
    void DrawAxisIndicator(const leon::Camera& camera, const ImVec2& imageMin,
                           const ImVec2& imageSize) const;
    [[nodiscard]] bool WorldPointUnderMouse(EditorContext& ctx, glm::vec3& outPoint) const;
};

} // namespace leon::editor
