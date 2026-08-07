#pragma once

#include <imgui.h>
#include <leon/editor/EditorContext.h>
#include <leon/editor/panels/AssetPreviewPanel.h>
#include <leon/editor/panels/ContentBrowserPanel.h>
#include <leon/editor/panels/DetailsPanel.h>
#include <leon/editor/panels/ImportDialog.h>
#include <leon/editor/panels/ConsolePanel.h>
#include <leon/editor/panels/MaterialEditorPanel.h>
#include <leon/editor/panels/OutlinerPanel.h>
#include <leon/editor/panels/OutputLogPanel.h>
#include <leon/editor/panels/ToolbarPanel.h>
#include <leon/editor/panels/ViewportPanel.h>
#include <leon/editor/panels/WorldSettingsPanel.h>
#include <leon/editor/panels/ProjectSettingsPanel.h>
#include <string>

namespace leon::editor {

/// Full-window docking host (menu bar + default Unreal-like panel layout).
class EditorLayout {
public:
    void Draw(EditorContext& ctx);

    [[nodiscard]] ViewportPanel& Viewport() { return viewport_; }

    /// Path used for ImGui docking / window layout (`editor_layout.ini` beside the exe).
    [[nodiscard]] static std::string LayoutIniPath();

private:
    void DrawMenuBar(EditorContext& ctx);
    void DrawPlaceActorsMenu(EditorContext& ctx);
    void DrawNewLevelDialog(EditorContext& ctx);
    void SetupDefaultDocking(EditorContext& ctx);
    void ApplyDefaultDockLayout(ImGuiID dockspaceId);
    [[nodiscard]] static bool HasUsableSavedLayout();
    [[nodiscard]] bool OpenLevel(EditorContext& ctx, const std::string& path);
    [[nodiscard]] bool SaveLevel(EditorContext& ctx, bool saveAs);
    /// Unreal-like: focused Material Editor if dirty, else current level if dirty.
    [[nodiscard]] bool SaveCurrent(EditorContext& ctx);
    /// Unreal-like Save All: dirty materials + dirty level.
    [[nodiscard]] bool SaveAll(EditorContext& ctx);
    [[nodiscard]] bool CreateNewLevel(EditorContext& ctx, int templateIndex);
    [[nodiscard]] bool HasUnsavedPackages(const EditorContext& ctx) const;

    ViewportPanel viewport_;
    OutlinerPanel outliner_;
    DetailsPanel details_;
    ContentBrowserPanel contentBrowser_;
    AssetPreviewPanel assetPreview_;
    ImportDialog importDialog_;
    ToolbarPanel toolbar_;
    WorldSettingsPanel worldSettings_;
    ProjectSettingsPanel projectSettings_;
    OutputLogPanel outputLog_;
    ConsolePanel console_;
    MaterialEditorPanel materialEditor_;
    bool dockingConfigured_ = false;
    bool forceDefaultLayout_ = false;
    /// Save ini a few frames after applying the built-in default (windows must dock first).
    int saveDefaultLayoutFrames_ = -1;
};

} // namespace leon::editor
