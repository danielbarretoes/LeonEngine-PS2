#include <glm/vec3.hpp>

#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <iterator>
#include <leon/core/Paths.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/AssetTools.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorBuild.h>
#include <leon/editor/EditorCommands.h>
#include <leon/editor/EditorFileDialog.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/EditorLayout.h>
#include <leon/editor/EditorLevelFactory.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/EditorToast.h>
#include <leon/editor/EngineContent.h>
#include <leon/editor/LevelSaver.h>
#include <leon/Engine.h>
#include <leon/level/BasicShape.h>
#include <leon/level/LevelLoader.h>
#include <leon/level/Light.h>
#include <leon/level/LightmapIO.h>
#include <string>

namespace leon::editor {

std::string EditorLayout::LayoutIniPath() {
    return (ExecutableDirectory() / "editor_layout.ini").lexically_normal().string();
}

bool EditorLayout::HasUsableSavedLayout() {
    const std::filesystem::path path = LayoutIniPath();
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        return false;
    }
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // A usable layout must include docking tree data, not only floating [Window] blocks.
    return contents.find("[Docking][Data]") != std::string::npos &&
           contents.find("DockSpace") != std::string::npos;
}

void EditorLayout::ApplyDefaultDockLayout(ImGuiID dockspaceId) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 size = (viewport != nullptr && viewport->WorkSize.x > 1.0f)
                            ? viewport->WorkSize
                            : ImVec2(1280.0f, 720.0f);

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);

    // Layout: large Viewport | right column (Outliner/World Settings + Details)
    //         bottom: Content Browser / Output Log (+ other utility tabs).
    ImGuiID dockMain = dockspaceId;
    ImGuiID dockRight =
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.26f, nullptr, &dockMain);
    ImGuiID dockBottom =
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.28f, nullptr, &dockMain);
    ImGuiID dockTop = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Up, 0.08f, nullptr, &dockMain);

    ImGuiID dockRightTop = dockRight;
    ImGuiID dockRightBottom =
        ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.50f, nullptr, &dockRightTop);

    ImGui::DockBuilderDockWindow("Toolbar", dockTop);
    ImGui::DockBuilderDockWindow("Viewport", dockMain);
    ImGui::DockBuilderDockWindow("World Outliner", dockRightTop);
    ImGui::DockBuilderDockWindow("World Settings", dockRightTop);
    ImGui::DockBuilderDockWindow("Details", dockRightBottom);
    ImGui::DockBuilderDockWindow("Content Browser", dockBottom);
    ImGui::DockBuilderDockWindow("Output Log", dockBottom);
    ImGui::DockBuilderDockWindow("Console", dockBottom);
    ImGui::DockBuilderDockWindow("Asset Preview", dockBottom);
    ImGui::DockBuilderDockWindow("Material Editor", dockBottom);
    ImGui::DockBuilderFinish(dockspaceId);
}

void EditorLayout::SetupDefaultDocking(EditorContext& ctx) {
    const ImGuiID dockspaceId = ImGui::GetID("LeonEditorDockspace");

    if (ctx.requestResetLayout) {
        ctx.requestResetLayout = false;
        dockingConfigured_ = false;
        forceDefaultLayout_ = true;
        saveDefaultLayoutFrames_ = -1;
    }

    // Wait until the host viewport has a real size — applying DockBuilder on a 0×0
    // first frame (welcome → editor) leaves every panel floating / scrambled.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workSize = viewport != nullptr ? viewport->WorkSize : ImVec2(0, 0);
    const bool sizeReady = workSize.x > 200.0f && workSize.y > 200.0f;

    if (!dockingConfigured_ && sizeReady) {
        const bool useSaved = !forceDefaultLayout_ && HasUsableSavedLayout();
        forceDefaultLayout_ = false;
        if (!useSaved) {
            ApplyDefaultDockLayout(dockspaceId);
            // Persist once windows have been submitted into the dock nodes.
            saveDefaultLayoutFrames_ = 2;
        }
        dockingConfigured_ = true;
    }

    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
}

bool EditorLayout::OpenLevel(EditorContext& ctx, const std::string& path) {
    if (ctx.engine == nullptr || path.empty()) {
        return false;
    }
    if (ctx.piePlaying) {
        ctx.requestPieStop = true;
        ctx.pendingOpenPath = path;
        return false;
    }
    if (HasUnsavedPackages(ctx)) {
        ctx.pendingOpenPath = path;
        return false;
    }
    if (!LoadLevelFile(*ctx.engine, path)) {
        return false;
    }
    ctx.levelPath = path;
    ctx.dirty = false;
    ctx.ClearSelection();
    ReloadLevelLightmapsForPath(*ctx.level, path);
    ctx.requestContentRefresh = true;
    if (ctx.history != nullptr) {
        ctx.history->Clear();
        ctx.history->Capture(ctx);
    }
    return true;
}

bool EditorLayout::SaveLevel(EditorContext& ctx, bool saveAs) {
    if (ctx.level == nullptr || ctx.camera == nullptr) {
        return false;
    }
    std::string path = ctx.levelPath;
    if (path.empty() || saveAs) {
        const std::string picked =
            EditorPickSaveFile("Leon Level\0*.llev\0All\0*.*\0", "Save Level As",
                               path.empty() ? "level.llev" : path.c_str());
        if (picked.empty()) {
            return false;
        }
        path = picked;
    }
    for (const StaticMeshComponent& mesh : ctx.level->StaticMeshes()) {
        if (mesh.tag == "Lava" || mesh.tag.rfind("Door:", 0) == 0) {
            std::cerr << "Editor: legacy mesh tag \"" << mesh.tag
                      << "\" — prefer TriggerVolume / PainCausingVolume PODs\n";
        }
    }
    RemapLevelPathsToContentRelative(ctx);
    if (!saveLevelFile(path, *ctx.level, *ctx.camera)) {
        std::cerr << "Editor: failed to save " << path << '\n';
        return false;
    }
    ctx.levelPath = path;
    ctx.dirty = false;
    if (ctx.catalog != nullptr && !ctx.projectPath.empty()) {
        (void)ctx.catalog->ScanPack(ctx.projectPath);
    }
    std::cout << "Editor: saved " << path << '\n';
    return true;
}

bool EditorLayout::HasUnsavedPackages(const EditorContext& ctx) const {
    return ctx.dirty || materialEditor_.HasDirtyDocs();
}

bool EditorLayout::SaveCurrent(EditorContext& ctx) {
    // Flow: Unreal-like Save (Ctrl+S)
    // 1. Focused dirty Material Editor tab
    // 2. Else dirty current level
    if (materialEditor_.HasFocusedDirtyDoc()) {
        return materialEditor_.SaveFocused(ctx);
    }
    if (ctx.dirty) {
        return SaveLevel(ctx, false);
    }
    return true;
}

bool EditorLayout::SaveAll(EditorContext& ctx) {
    // Flow: Unreal-like Save All (Ctrl+Shift+S)
    bool ok = materialEditor_.SaveAll(ctx);
    if (ctx.dirty) {
        ok = SaveLevel(ctx, false) && ok;
    }
    return ok;
}

void EditorLayout::DrawPlaceActorsMenu(EditorContext& ctx) {
    if (ctx.level == nullptr || ctx.resources == nullptr) {
        return;
    }
    if (!ImGui::BeginMenu("Place Actors")) {
        return;
    }

    auto placeShape = [&](EBasicShape shape, const char* className) {
        if (!ImGui::MenuItem(className)) {
            return;
        }
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
        glm::vec3 pos = ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0.5f, 0};
        if (ctx.snapEnabled) {
            Transform t{};
            t.position = pos;
            EditorCommands::SnapTransform(t, ctx.gridSize, ctx.rotationSnapDegrees, false);
            pos = t.position;
        }
        std::string err;
        if (!PlaceBasicShapeActor(ctx, shape, pos, err) && !err.empty()) {
            std::cerr << "Editor: Place " << className << " failed: " << err << '\n';
        }
    };

    placeShape(EBasicShape::Cube, "Cube");
    placeShape(EBasicShape::Sphere, "Sphere");
    placeShape(EBasicShape::Plane, "Plane");
    if (ImGui::MenuItem("BlockingVolume")) {
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
        glm::vec3 pos = ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0.5f, 0};
        if (ctx.snapEnabled) {
            Transform t{};
            t.position = pos;
            EditorCommands::SnapTransform(t, ctx.gridSize, ctx.rotationSnapDegrees, false);
            pos = t.position;
        }
        BasicShape basic;
        basic.type = EBasicShape::Cube;
        basic.transform.position = pos;
        basic.transform.position.y = pos.y + 0.5f;
        StaticMeshComponent mesh = basic.MakeStaticMesh(*ctx.resources);
        mesh.editorClass = "BlockingVolume";
        mesh.hidden = true;
        mesh.collisionEnabled = true;
        mesh.materialPath = "Materials/M_Default.lmat";
        mesh.material = ctx.resources->LoadMaterial(ResolveAssetPath(mesh.materialPath));
        if (!mesh.material.albedoMap) {
            mesh.material = ctx.resources->DefaultMaterial();
        }
        mesh.material.castsShadows = false;
        mesh.materialOverride = true;
        ctx.level->AddStaticMesh(std::move(mesh));
        ctx.Select(EEditorSelectionKind::StaticMesh, ctx.level->StaticMeshes().size() - 1);
        ctx.MarkDirty();
    }
    if (ImGui::MenuItem("StaticMesh…")) {
        const std::string picked =
            EditorPickOpenFile("Leon Static Mesh\0*.lmesh\0All\0*.*\0", "Place StaticMesh");
        if (!picked.empty()) {
            if (ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            const std::string meshPath = MakePackRelativeAssetPath(ctx, picked);
            auto loaded = ctx.resources->LoadStaticMesh(ResolveAssetPath(meshPath));
            if (loaded == nullptr || !loaded->Valid()) {
                loaded = ctx.resources->LoadStaticMesh(picked);
            }
            if (loaded == nullptr || !loaded->Valid()) {
                std::cerr << "Editor: failed to load StaticMesh " << picked << '\n';
            } else {
                glm::vec3 pos =
                    ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0.5f, 0};
                if (ctx.snapEnabled) {
                    Transform t{};
                    t.position = pos;
                    EditorCommands::SnapTransform(t, ctx.gridSize, ctx.rotationSnapDegrees, false);
                    pos = t.position;
                }
                StaticMeshComponent actor;
                actor.mesh = std::move(loaded);
                actor.transform.position = pos;
                actor.editorClass = "StaticMesh";
                actor.meshPath = meshPath;
                actor.material = ctx.resources->DefaultMaterial();
                ctx.level->AddStaticMesh(std::move(actor));
                ctx.Select(EEditorSelectionKind::StaticMesh, ctx.level->StaticMeshes().size() - 1);
                ctx.MarkDirty();
            }
        }
    }
    if (ImGui::MenuItem("PlayerStart")) {
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
        PlayerStart start{};
        start.transform.position =
            ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0, 0};
        ctx.level->AddPlayerStart(start);
        ctx.Select(EEditorSelectionKind::PlayerStart, ctx.level->PlayerStarts().size() - 1);
        ctx.MarkDirty();
    }
    if (ImGui::MenuItem("TriggerVolume")) {
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
        TriggerVolume volume{};
        volume.transform.position =
            ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0, 0};
        volume.transform.scale = {2.0f, 2.0f, 2.0f};
        volume.interactRadius = 2.0f;
        volume.interactCost = 0;
        volume.payload = "Door";
        ctx.level->AddTriggerVolume(volume);
        ctx.Select(EEditorSelectionKind::TriggerVolume, ctx.level->TriggerVolumes().size() - 1);
        ctx.MarkDirty();
    }
    if (ImGui::MenuItem("PainCausingVolume")) {
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
        PainCausingVolume volume{};
        volume.transform.position =
            ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0, 0};
        volume.transform.scale = {4.0f, 0.5f, 4.0f};
        volume.damagePerSecond = 12.0f;
        volume.damageInterval = 0.35f;
        ctx.level->AddPainCausingVolume(volume);
        ctx.Select(EEditorSelectionKind::PainCausingVolume,
                   ctx.level->PainCausingVolumes().size() - 1);
        ctx.MarkDirty();
    }
    if (ImGui::MenuItem("AISpawnPoint")) {
        if (ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
        AISpawnPoint point{};
        point.transform.position =
            ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0, 0};
        ctx.level->AddAISpawnPoint(point);
        ctx.Select(EEditorSelectionKind::AISpawnPoint, ctx.level->AISpawnPoints().size() - 1);
        ctx.MarkDirty();
    }
    if (ImGui::MenuItem("PointLight")) {
        if (ctx.level->PointLights().size() >= static_cast<std::size_t>(kMaxPointLights)) {
            EditorToast("Max PointLights (" + std::to_string(kMaxPointLights) + ") reached",
                        EEditorToastKind::Warning, 3.0f);
            std::cerr << "Editor: max point lights (" << kMaxPointLights << ") reached\n";
        } else {
            if (ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            PointLight light{};
            light.transform.position = ctx.camera != nullptr
                                           ? ctx.camera->Target() + glm::vec3{0, 2, 0}
                                           : glm::vec3{0, 2, 0};
            ctx.level->PointLights().push_back(light);
            ctx.Select(EEditorSelectionKind::PointLight, ctx.level->PointLights().size() - 1);
            ctx.MarkDirty();
        }
    }
    if (ImGui::MenuItem("DirectionalLight")) {
        if (ctx.level->DirectionalLights().size() >=
            static_cast<std::size_t>(kMaxDirectionalLights)) {
            EditorToast("Max DirectionalLights (" + std::to_string(kMaxDirectionalLights) +
                            ") reached",
                        EEditorToastKind::Warning, 3.0f);
            std::cerr << "Editor: max directional lights (" << kMaxDirectionalLights
                      << ") reached\n";
        } else {
            if (ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            DirectionalLight light{};
            light.transform.position = ctx.camera != nullptr
                                           ? ctx.camera->Target() + glm::vec3{0, 4, 0}
                                           : glm::vec3{0, 4, 0};
            ctx.level->DirectionalLights().push_back(light);
            ctx.Select(EEditorSelectionKind::DirectionalLight,
                       ctx.level->DirectionalLights().size() - 1);
            ctx.MarkDirty();
        }
    }
    ImGui::EndMenu();
}

void EditorLayout::DrawMenuBar(EditorContext& ctx) {
    if (!ImGui::BeginMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Level…", "Ctrl+N")) {
            ctx.requestNewLevelDialog = true;
        }
        if (ImGui::MenuItem("Import Asset…", "Ctrl+I")) {
            ctx.requestOpenImportDialog = true;
        }
        if (ImGui::BeginMenu("Open Level")) {
            if (ctx.catalog != nullptr) {
                for (const LevelEntry& entry : ctx.catalog->Entries()) {
                    const std::string label = entry.pack + " / " + entry.name;
                    if (ImGui::MenuItem(label.c_str())) {
                        (void)OpenLevel(ctx, entry.path);
                    }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            (void)SaveCurrent(ctx);
        }
        if (ImGui::MenuItem("Save All", "Ctrl+Shift+S")) {
            (void)SaveAll(ctx);
        }
        if (ImGui::MenuItem("Save Current Level As…")) {
            (void)SaveLevel(ctx, true);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Build Game…", "Ctrl+B", false,
                            !ctx.projectPath.empty() && !EditorBuild::IsBusy())) {
            ctx.requestBuildProject = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Close Project")) {
            if (HasUnsavedPackages(ctx)) {
                ctx.pendingCloseProject = true;
                ctx.pendingExit = false;
            } else {
                ctx.requestCloseProject = true;
            }
        }
        if (ImGui::MenuItem("Exit")) {
            if (HasUnsavedPackages(ctx)) {
                ctx.pendingExit = true;
                ctx.pendingCloseProject = false;
            } else if (ctx.engine != nullptr) {
                ctx.engine->RequestQuit();
            }
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        const bool canUndo = ctx.history != nullptr && ctx.history->CanUndo();
        const bool canRedo = ctx.history != nullptr && ctx.history->CanRedo();
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo && ctx.engine != nullptr)) {
            (void)ctx.history->Undo(*ctx.engine, ctx);
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo && ctx.engine != nullptr)) {
            (void)ctx.history->Redo(*ctx.engine, ctx);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, ctx.selection.IsValid())) {
            EditorCommands::DuplicateSelection(ctx, ctx.history);
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, ctx.selection.IsValid())) {
            EditorCommands::CopySelection(ctx);
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, EditorCommands::HasClipboard())) {
            EditorCommands::PasteClipboard(ctx, ctx.history);
        }
        if (ImGui::MenuItem("Delete", "Del", false, ctx.selection.IsValid())) {
            EditorCommands::DeleteSelection(ctx, ctx.history);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Deselect", "Esc")) {
            ctx.ClearSelection();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Project Settings…", nullptr, false, !ctx.projectPath.empty())) {
            ctx.showProjectSettings = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Focus Selected", "F", false, ctx.selection.IsValid())) {
            ctx.RequestFocusSelected();
        }
        ImGui::MenuItem("Show Grid", nullptr, &ctx.showGrid);
        ImGui::MenuItem("Show Stats", nullptr, &ctx.showStats);
        ImGui::Separator();
        if (ImGui::MenuItem("Perspective", "Alt+1", ctx.viewMode == EEditorViewMode::Perspective)) {
            ctx.viewMode = EEditorViewMode::Perspective;
        }
        if (ImGui::MenuItem("Top (Ortho)", "Alt+2", ctx.viewMode == EEditorViewMode::OrthoTop)) {
            ctx.viewMode = EEditorViewMode::OrthoTop;
        }
        if (ImGui::MenuItem("Front (Ortho)", "Alt+3",
                            ctx.viewMode == EEditorViewMode::OrthoFront)) {
            ctx.viewMode = EEditorViewMode::OrthoFront;
        }
        if (ImGui::MenuItem("Side (Ortho)", "Alt+4", ctx.viewMode == EEditorViewMode::OrthoSide)) {
            ctx.viewMode = EEditorViewMode::OrthoSide;
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("View Mode")) {
            if (ImGui::MenuItem("Lit", "Alt+5",
                                ctx.viewportViewMode == EEditorViewportViewMode::Lit)) {
                ctx.viewportViewMode = EEditorViewportViewMode::Lit;
            }
            if (ImGui::MenuItem("Player Collision", "Alt+6",
                                ctx.viewportViewMode ==
                                    EEditorViewportViewMode::PlayerCollision)) {
                ctx.viewportViewMode = EEditorViewportViewMode::PlayerCollision;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    DrawPlaceActorsMenu(ctx);
    if (ImGui::BeginMenu("Build")) {
        const bool canBuild = !ctx.projectPath.empty() && !EditorBuild::IsBusy();
        if (ImGui::MenuItem("Build Game", "Ctrl+B", false, canBuild)) {
            ctx.requestBuildProject = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Compile the open game.\n"
                              "Output: <YourProject>/Shipping/leon-<Name>.exe\n"
                              "(intermediate files go to <YourProject>/build-fast/)");
        }
        if (EditorBuild::IsBusy()) {
            ImGui::TextDisabled("Building… (see Output Log)");
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem("Console", "`", &ctx.showConsole);
        ImGui::MenuItem("Output Log", nullptr, &ctx.showOutputLog);
        ImGui::MenuItem("Material Editor", nullptr, &ctx.showMaterialEditor);
        ImGui::MenuItem("Content Browser", nullptr, &ctx.showContentBrowser);
        ImGui::MenuItem("Asset Preview", nullptr, &ctx.showAssetPreview);
        ImGui::MenuItem("World Outliner", nullptr, &ctx.showOutliner);
        ImGui::MenuItem("Details", nullptr, &ctx.showDetails);
        ImGui::MenuItem("World Settings", nullptr, &ctx.showWorldSettings);
        ImGui::MenuItem("Project Settings", nullptr, &ctx.showProjectSettings);
        ImGui::Separator();
        if (ImGui::MenuItem("Save Layout")) {
            ctx.requestSaveLayout = true;
        }
        if (ImGui::MenuItem("Reset Layout")) {
            ctx.requestResetLayout = true;
        }
        ImGui::Separator();
        ImGui::TextDisabled("Drag window tabs to dock like Unreal");
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();

    // Global shortcuts
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
        ctx.requestNewLevelDialog = true;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && ctx.history != nullptr &&
        ctx.engine != nullptr) {
        (void)ctx.history->Undo(*ctx.engine, ctx);
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y) && ctx.history != nullptr &&
        ctx.engine != nullptr) {
        (void)ctx.history->Redo(*ctx.engine, ctx);
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        EditorCommands::DuplicateSelection(ctx, ctx.history);
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        EditorCommands::CopySelection(ctx);
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
        EditorCommands::PasteClipboard(ctx, ctx.history);
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (io.KeyShift) {
            (void)SaveAll(ctx);
        } else {
            (void)SaveCurrent(ctx);
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B) && !ctx.projectPath.empty() &&
        !EditorBuild::IsBusy()) {
        ctx.requestBuildProject = true;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_I)) {
        ctx.requestOpenImportDialog = true;
    }
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape) && !ctx.piePlaying) {
        ctx.ClearSelection();
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_1)) {
        ctx.viewMode = EEditorViewMode::Perspective;
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_2)) {
        ctx.viewMode = EEditorViewMode::OrthoTop;
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_3)) {
        ctx.viewMode = EEditorViewMode::OrthoFront;
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_4)) {
        ctx.viewMode = EEditorViewMode::OrthoSide;
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_5)) {
        ctx.viewportViewMode = EEditorViewportViewMode::Lit;
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_6)) {
        ctx.viewportViewMode = EEditorViewportViewMode::PlayerCollision;
    }
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_GraveAccent)) {
        ctx.showConsole = true;
        ctx.requestFocusConsole = true;
    }
}

bool EditorLayout::CreateNewLevel(EditorContext& ctx, int templateIndex) {
    if (ctx.engine == nullptr) {
        return false;
    }
    if (ctx.piePlaying) {
        ctx.requestPieStop = true;
        ctx.pendingNewLevelTemplate = templateIndex;
        return false;
    }
    std::string err;
    const ENewLevelTemplate tmpl = templateIndex == static_cast<int>(ENewLevelTemplate::Starter)
                                       ? ENewLevelTemplate::Starter
                                       : ENewLevelTemplate::Blank;
    if (!CreateLevelFromTemplate(tmpl, *ctx.engine, err)) {
        std::cerr << "Editor: New Level failed: " << err << '\n';
        return false;
    }
    if (!err.empty()) {
        std::cerr << "Editor: New Level warning: " << err << '\n';
    }
    ctx.levelPath.clear();
    ctx.dirty = true;
    ctx.ClearSelection();
    ctx.requestContentRefresh = true;
    ctx.pendingNewLevelTemplate = -1;
    if (ctx.history != nullptr) {
        ctx.history->Clear();
        ctx.history->Capture(ctx);
    }
    return true;
}

void EditorLayout::DrawNewLevelDialog(EditorContext& ctx) {
    if (ctx.requestNewLevelDialog) {
        ctx.requestNewLevelDialog = false;
        ImGui::OpenPopup("New Level");
    }

    if (!ImGui::BeginPopupModal("New Level", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextUnformatted("Choose a starting template");
    ImGui::Spacing();

    static int selected = 0;
    if (ImGui::RadioButton("Blank — empty level with sunlight", selected == 0)) {
        selected = 0;
    }
    if (ImGui::RadioButton("Starter — floor, PlayerStart, light, skybox", selected == 1)) {
        selected = 1;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("Create", ImVec2(120.0f, 0.0f))) {
        if (HasUnsavedPackages(ctx)) {
            ctx.pendingNewLevelTemplate = selected;
            ImGui::CloseCurrentPopup();
        } else {
            (void)CreateNewLevel(ctx, selected);
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void EditorLayout::Draw(EditorContext& ctx) {
    ctx.ResolveSelectionIndices();

    // Content Browser / menus may request Save Current / Save All / Save asset.
    if (!ctx.requestSaveAssetPath.empty()) {
        const std::string path = ctx.requestSaveAssetPath;
        ctx.requestSaveAssetPath.clear();
        namespace fs = std::filesystem;
        std::string ext = fs::path(path).extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext == ".lmat") {
            if (!materialEditor_.IsPathDirty(path)) {
                materialEditor_.OpenMaterial(path);
            }
            (void)materialEditor_.SavePath(ctx, path);
        } else if (ext == ".llev") {
            if (!ctx.levelPath.empty() && fs::path(ctx.levelPath).lexically_normal() ==
                                             fs::path(path).lexically_normal() &&
                ctx.dirty) {
                (void)SaveLevel(ctx, false);
            } else if (ctx.dirty && ctx.levelPath.empty()) {
                (void)SaveLevel(ctx, false);
            }
        }
    }
    if (ctx.requestSaveAll) {
        ctx.requestSaveAll = false;
        (void)SaveAll(ctx);
    }
    if (ctx.requestSaveCurrent) {
        ctx.requestSaveCurrent = false;
        (void)SaveCurrent(ctx);
    }

    // Flow: Content Browser Asset Tools (Rename / Move / Delete)
    if (!ctx.requestMaterialEditorRemapFrom.empty()) {
        materialEditor_.RemapAssetPath(ctx.requestMaterialEditorRemapFrom,
                                       ctx.requestMaterialEditorRemapTo);
        ctx.requestMaterialEditorRemapFrom.clear();
        ctx.requestMaterialEditorRemapTo.clear();
    }
    if (!ctx.requestRenameAssetPath.empty()) {
        const std::string path = ctx.requestRenameAssetPath;
        const std::string name = ctx.pendingRenameAssetName;
        ctx.requestRenameAssetPath.clear();
        ctx.pendingRenameAssetName.clear();
        const AssetToolsResult result = RenameAsset(ctx, path, name);
        if (result.ok) {
            EditorToast("Renamed " + name, EEditorToastKind::Success, 2.5f);
            if (!ctx.requestMaterialEditorRemapFrom.empty()) {
                materialEditor_.RemapAssetPath(ctx.requestMaterialEditorRemapFrom,
                                               ctx.requestMaterialEditorRemapTo);
                ctx.requestMaterialEditorRemapFrom.clear();
                ctx.requestMaterialEditorRemapTo.clear();
            }
        } else {
            EditorToast(result.error.empty() ? "Rename failed" : result.error,
                        EEditorToastKind::Error, 4.0f);
        }
    }
    if (!ctx.requestMoveAssetPath.empty() && !ctx.requestMoveAssetDestFolder.empty()) {
        const std::string from = ctx.requestMoveAssetPath;
        const std::string dest = ctx.requestMoveAssetDestFolder;
        ctx.requestMoveAssetPath.clear();
        ctx.requestMoveAssetDestFolder.clear();
        const AssetToolsResult result = MoveAsset(ctx, from, dest);
        if (result.ok) {
            EditorToast("Moved asset", EEditorToastKind::Success, 2.5f);
            if (!ctx.requestMaterialEditorRemapFrom.empty()) {
                materialEditor_.RemapAssetPath(ctx.requestMaterialEditorRemapFrom,
                                               ctx.requestMaterialEditorRemapTo);
                ctx.requestMaterialEditorRemapFrom.clear();
                ctx.requestMaterialEditorRemapTo.clear();
            }
        } else {
            EditorToast(result.error.empty() ? "Move failed" : result.error,
                        EEditorToastKind::Error, 4.0f);
        }
    }
    if (!ctx.requestDeleteAssetPath.empty()) {
        const std::string path = ctx.requestDeleteAssetPath;
        ctx.requestDeleteAssetPath.clear();
        const AssetToolsResult result = DeleteAssets(ctx, path);
        if (result.ok) {
            EditorToast("Deleted asset", EEditorToastKind::Success, 2.5f);
            if (!ctx.requestMaterialEditorRemapFrom.empty()) {
                materialEditor_.RemapAssetPath(ctx.requestMaterialEditorRemapFrom,
                                               ctx.requestMaterialEditorRemapTo);
                ctx.requestMaterialEditorRemapFrom.clear();
                ctx.requestMaterialEditorRemapTo.clear();
            }
        } else {
            EditorToast(result.error.empty() ? "Delete failed" : result.error,
                        EEditorToastKind::Error, 4.0f);
        }
    }

    if (ctx.requestBuildProject) {
        ctx.requestBuildProject = false;
        if (HasUnsavedPackages(ctx)) {
            (void)SaveAll(ctx);
        }
        ctx.showOutputLog = true;
        ctx.requestFocusOutputLog = true;
        if (EditorBuild::StartProjectBuild(ctx.projectPath, ctx.projectName,
                                           ctx.projectBuildDedicatedServer)) {
            EditorToast("Building game…", EEditorToastKind::Info, 4.0f);
        } else {
            EditorToast("Build failed to start", EEditorToastKind::Error, 4.5f);
        }
    }
    if (!ctx.piePlaying && !ctx.pendingOpenPath.empty() && !HasUnsavedPackages(ctx) &&
        !ImGui::IsPopupOpen("Unsaved Changes")) {
        const std::string path = ctx.pendingOpenPath;
        ctx.pendingOpenPath.clear();
        (void)OpenLevel(ctx, path);
    }
    if (!ctx.piePlaying && ctx.pendingNewLevelTemplate >= 0 && !HasUnsavedPackages(ctx) &&
        !ImGui::IsPopupOpen("Unsaved Changes") && !ImGui::IsPopupOpen("New Level")) {
        const int tmpl = ctx.pendingNewLevelTemplate;
        ctx.pendingNewLevelTemplate = -1;
        (void)CreateNewLevel(ctx, tmpl);
    }
    if (!ctx.piePlaying && ctx.pendingCloseProject && !HasUnsavedPackages(ctx) &&
        !ImGui::IsPopupOpen("Unsaved Changes")) {
        ctx.pendingCloseProject = false;
        ctx.requestCloseProject = true;
    }
    if (!ctx.piePlaying && ctx.pendingExit && !HasUnsavedPackages(ctx) &&
        !ImGui::IsPopupOpen("Unsaved Changes")) {
        ctx.pendingExit = false;
        if (ctx.engine != nullptr) {
            ctx.engine->RequestQuit();
        }
    }

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("LeonEditorHost", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    DrawMenuBar(ctx);

    SetupDefaultDocking(ctx);

    toolbar_.Draw(ctx);
    viewport_.Draw(ctx);
    outliner_.Draw(ctx);
    details_.Draw(ctx);
    contentBrowser_.Draw(ctx);
    assetPreview_.Draw(ctx);
    worldSettings_.Draw(ctx);
    projectSettings_.Draw(ctx);
    outputLog_.Draw(ctx);
    console_.Draw(ctx);
    materialEditor_.Draw(ctx);

    if (ctx.requestOpenImportDialog) {
        ctx.requestOpenImportDialog = false;
        importDialog_.Open();
    }
    importDialog_.Draw(ctx);
    DrawNewLevelDialog(ctx);

    // Dirty confirmation (open level / new level / close project / exit).
    if ((!ctx.pendingOpenPath.empty() || ctx.pendingNewLevelTemplate >= 0 ||
         ctx.pendingCloseProject || ctx.pendingExit) &&
        HasUnsavedPackages(ctx) && !ImGui::IsPopupOpen("Unsaved Changes") &&
        !ImGui::IsPopupOpen("New Level")) {
        ImGui::OpenPopup("Unsaved Changes");
    }
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ctx.pendingExit) {
            ImGui::TextUnformatted("You have unsaved packages. Save All before exiting?");
        } else if (ctx.pendingCloseProject) {
            ImGui::TextUnformatted("You have unsaved packages. Save All before closing?");
        } else if (ctx.pendingNewLevelTemplate >= 0) {
            ImGui::TextUnformatted("You have unsaved packages. Discard and create a new level?");
        } else {
            ImGui::TextUnformatted("You have unsaved packages. Discard and open the new level?");
        }
        if (ImGui::Button("Discard", ImVec2(120, 0))) {
            ctx.dirty = false;
            ctx.dirtyMaterialPaths.clear();
            // Drop dirty material tabs without writing (Unreal discard package).
            materialEditor_ = MaterialEditorPanel{};
            ImGui::CloseCurrentPopup();
            if (ctx.pendingExit) {
                ctx.pendingExit = false;
                if (ctx.engine != nullptr) {
                    ctx.engine->RequestQuit();
                }
            } else if (ctx.pendingCloseProject) {
                ctx.pendingCloseProject = false;
                ctx.requestCloseProject = true;
            } else if (ctx.pendingNewLevelTemplate >= 0) {
                const int tmpl = ctx.pendingNewLevelTemplate;
                ctx.pendingNewLevelTemplate = -1;
                (void)CreateNewLevel(ctx, tmpl);
            } else {
                const std::string path = ctx.pendingOpenPath;
                ctx.pendingOpenPath.clear();
                (void)OpenLevel(ctx, path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save All", ImVec2(120, 0))) {
            if (SaveAll(ctx)) {
                ImGui::CloseCurrentPopup();
                if (ctx.pendingExit) {
                    ctx.pendingExit = false;
                    if (ctx.engine != nullptr) {
                        ctx.engine->RequestQuit();
                    }
                } else if (ctx.pendingCloseProject) {
                    ctx.pendingCloseProject = false;
                    ctx.requestCloseProject = true;
                } else if (ctx.pendingNewLevelTemplate >= 0) {
                    const int tmpl = ctx.pendingNewLevelTemplate;
                    ctx.pendingNewLevelTemplate = -1;
                    (void)CreateNewLevel(ctx, tmpl);
                } else {
                    const std::string path = ctx.pendingOpenPath;
                    ctx.pendingOpenPath.clear();
                    (void)OpenLevel(ctx, path);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ctx.pendingOpenPath.clear();
            ctx.pendingNewLevelTemplate = -1;
            ctx.pendingCloseProject = false;
            ctx.pendingExit = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();

    DrawEditorToasts();

    if (saveDefaultLayoutFrames_ >= 0) {
        --saveDefaultLayoutFrames_;
        if (saveDefaultLayoutFrames_ < 0) {
            ctx.requestSaveLayout = true;
        }
    }

    if (ctx.requestSaveLayout) {
        ctx.requestSaveLayout = false;
        if (const char* ini = ImGui::GetIO().IniFilename; ini != nullptr && ini[0] != '\0') {
            ImGui::SaveIniSettingsToDisk(ini);
            std::cout << "Editor: saved layout to " << ini << '\n';
        }
    }
}

} // namespace leon::editor
