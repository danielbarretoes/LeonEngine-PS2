#include <leon/editor/panels/WorldSettingsPanel.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <imgui.h>
#include <leon/core/Paths.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/EditorProject.h>
#include <leon/editor/EngineContent.h>
#include <leon/level/Level.h>
#include <leon/render/PostProcess.h>
#include <leon/render/Renderer.h>
#include <leon/render/ResourceCache.h>

namespace leon::editor {
namespace {

[[nodiscard]] std::string BasenameLabel(const std::string& path) {
    if (path.empty()) {
        return "(none)";
    }
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

void PersistEditorPostProcess(EditorContext& ctx) {
    if (ctx.projectPath.empty() || ctx.renderer == nullptr) {
        return;
    }
    std::string err;
    if (!EditorProjectService::WriteProjectEditorPostProcess(
            ctx.projectPath, ctx.renderer->GetPostProcessSettings(), err)) {
        std::cerr << "Editor: failed to save editorPostProcess: " << err << '\n';
    }
}

} // namespace

void WorldSettingsPanel::RefreshCatalogs(EditorContext& ctx) {
    if (ctx.requestContentRefresh) {
        catalogsProjectKey_.clear();
    }
    if (!catalogsProjectKey_.empty() && catalogsProjectKey_ == ctx.projectPath) {
        return;
    }
    catalogsProjectKey_ = ctx.projectPath;
    skyboxes_ = CollectEditorSkyboxes(ctx.projectPath);
}

void WorldSettingsPanel::Draw(EditorContext& ctx) {
    if (!ctx.showWorldSettings) {
        return;
    }
    if (!ImGui::Begin("World Settings", &ctx.showWorldSettings)) {
        ImGui::End();
        return;
    }

    RefreshCatalogs(ctx);

    if (ctx.level == nullptr || ctx.resources == nullptr) {
        ImGui::TextDisabled("Open a level to edit World Settings");
        ImGui::End();
        return;
    }

    Level& level = *ctx.level;

    if (ImGui::CollapsingHeader("Level", ImGuiTreeNodeFlags_DefaultOpen)) {
        char nameBuf[128];
        (void)std::snprintf(nameBuf, sizeof(nameBuf), "%s", level.Name().c_str());
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            level.SetName(nameBuf);
            ctx.MarkDirty();
        }
        if (ImGui::IsItemActivated() && ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }

        const std::vector<std::string> modes = ListEditorGameModes(ctx.projectPath);
        std::string current = level.GameMode();
        // Legacy editor-internal id — treat as third-person for the dropdown.
        if (current == "Pie" || current == "pie") {
            current = "third-person";
        }
        if (current.empty()) {
            current = "third-person";
        }
        bool currentKnown = false;
        for (const std::string& mode : modes) {
            if (mode == current) {
                currentKnown = true;
                break;
            }
        }
        const std::string comboPreview = currentKnown ? current : (current + " (custom)");
        if (ImGui::BeginCombo("GameMode Override", comboPreview.c_str())) {
            for (const std::string& mode : modes) {
                const bool selected = (mode == current);
                if (ImGui::Selectable(mode.c_str(), selected)) {
                    if (ctx.history != nullptr) {
                        ctx.history->Capture(ctx);
                    }
                    level.SetGameMode(mode);
                    ctx.MarkDirty();
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled(
            "Default = free-look PIE. third-person / Pie = Character PIE. "
            "Pack ids (e.g. coop-tp) = third-person Pie preview in editor; "
            "full GameMode only in the shipping exe (Build Game).");

        char gmBuf[128];
        (void)std::snprintf(gmBuf, sizeof(gmBuf), "%s", level.GameMode().c_str());
        if (ImGui::InputText("Custom Id", gmBuf, sizeof(gmBuf))) {
            level.SetGameMode(gmBuf);
            ctx.MarkDirty();
        }
        if (ImGui::IsItemActivated() && ctx.history != nullptr) {
            ctx.history->Capture(ctx);
        }
    }

    if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        const std::string currentSky = level.EnvironmentPath();
        const std::string skyPreview = BasenameLabel(currentSky);
        if (ImGui::BeginCombo("Skybox", skyPreview.c_str())) {
            const bool noneSelected = currentSky.empty();
            if (ImGui::Selectable("(none)", noneSelected)) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                level.SetEnvironment(nullptr);
                level.SetEnvironmentPath({});
                ctx.MarkDirty();
            }
            for (const EditorSkyboxEntry& entry : skyboxes_) {
                const bool selected = entry.authoringPath == currentSky ||
                                      entry.absolutePath == currentSky ||
                                      BasenameLabel(entry.authoringPath) == BasenameLabel(currentSky);
                if (ImGui::Selectable(entry.displayName.c_str(), selected)) {
                    if (ctx.history != nullptr) {
                        ctx.history->Capture(ctx);
                    }
                    const std::string loadPath =
                        entry.absolutePath.empty() ? ResolveAssetPath(entry.authoringPath)
                                                   : entry.absolutePath;
                    auto env = ctx.resources->LoadEnvMap(loadPath);
                    if (env) {
                        level.SetEnvironment(std::move(env));
                        level.SetEnvironmentPath(
                            MakePackRelativeAssetPath(ctx, entry.authoringPath));
                        ctx.MarkDirty();
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", entry.authoringPath.c_str());
                }
            }
            ImGui::EndCombo();
        }

        float exposure = level.EnvironmentExposure();
        if (ImGui::DragFloat("Exposure", &exposure, 0.01f, 0.05f, 8.0f)) {
            if (ImGui::IsItemActivated() && ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            level.SetEnvironmentExposure(exposure);
            ctx.MarkDirty();
        }

        if (ctx.renderer != nullptr) {
            ImGui::SeparatorText("Post Process");
            ImGui::TextDisabled("Project preference (leon.game.json) — not per-level");
            PostProcessSettings& post = ctx.renderer->GetPostProcessSettings();
            int quality = static_cast<int>(post.quality);
            const char* qualities[] = {"Off", "Low", "Medium", "High"};
            if (ImGui::Combo("Quality", &quality, qualities, 4)) {
                ctx.renderer->SetPostProcessQuality(static_cast<EPostProcessQuality>(quality));
                PersistEditorPostProcess(ctx);
            }
            bool enabled = post.enabled;
            if (ImGui::Checkbox("Enabled", &enabled)) {
                ctx.renderer->SetPostProcessEnabled(enabled);
                PersistEditorPostProcess(ctx);
            }
            bool ao = post.ambientOcclusion;
            if (ImGui::Checkbox("Ambient Occlusion", &ao)) {
                ctx.renderer->SetAmbientOcclusionEnabled(ao);
                PersistEditorPostProcess(ctx);
            }
            bool fxaa = post.fxaa;
            if (ImGui::Checkbox("FXAA", &fxaa)) {
                ctx.renderer->SetFxaaEnabled(fxaa);
                PersistEditorPostProcess(ctx);
            }
            bool earlyZ = post.earlyZ;
            if (ImGui::Checkbox("Early-Z", &earlyZ)) {
                ctx.renderer->SetEarlyZEnabled(earlyZ);
                PersistEditorPostProcess(ctx);
            }
            ImGui::DragFloat("AO Intensity", &post.aoIntensity, 0.01f, 0.0f, 2.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                PersistEditorPostProcess(ctx);
            }
            ImGui::DragFloat("AO Radius", &post.aoRadius, 0.01f, 0.05f, 2.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                PersistEditorPostProcess(ctx);
            }
            ImGui::DragFloat("AO Bias", &post.aoBias, 0.001f, 0.0f, 0.2f);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                PersistEditorPostProcess(ctx);
            }
            ImGui::DragFloat("AO Power", &post.aoPower, 0.01f, 0.25f, 4.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                PersistEditorPostProcess(ctx);
            }
            if (ImGui::RadioButton("Shadow 1024", post.shadowMapSize == 1024)) {
                post.shadowMapSize = 1024;
                PersistEditorPostProcess(ctx);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Shadow 2048", post.shadowMapSize == 2048)) {
                post.shadowMapSize = 2048;
                PersistEditorPostProcess(ctx);
            }
        }

        if (ctx.requestPickHdr && !ctx.pendingHdrPickPath.empty()) {
            if (ctx.history != nullptr) {
                ctx.history->Capture(ctx);
            }
            const std::string path = MakePackRelativeAssetPath(ctx, ctx.pendingHdrPickPath);
            ctx.pendingHdrPickPath.clear();
            ctx.requestPickHdr = false;
            auto env = ctx.resources->LoadEnvMap(ResolveAssetPath(path));
            if (!env) {
                env = ctx.resources->LoadEnvMap(path);
            }
            if (env) {
                level.SetEnvironment(std::move(env));
                level.SetEnvironmentPath(path);
                ctx.MarkDirty();
                catalogsProjectKey_.clear();
            }
        }
        if (ImGui::Button("Pick HDR from Content…")) {
            ctx.requestPickHdr = true;
            ImGui::OpenPopup("Pick HDR hint");
        }
        if (ImGui::BeginPopup("Pick HDR hint")) {
            ImGui::TextUnformatted("Double-click or drag an HDR from Content Browser.");
            if (ImGui::Button("OK")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    if (ImGui::CollapsingHeader("Editor Grid", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Grid", &ctx.showGrid);
        ImGui::Checkbox("Snap Enabled", &ctx.snapEnabled);
        ImGui::DragFloat("Grid Size", &ctx.gridSize, 0.05f, 0.05f, 50.0f);
        ImGui::DragFloat("Rotation Snap", &ctx.rotationSnapDegrees, 1.0f, 1.0f, 90.0f);
        int view = static_cast<int>(ctx.viewMode);
        const char* views[] = {"Perspective", "Top (Ortho)", "Front (Ortho)", "Side (Ortho)"};
        if (ImGui::Combo("Viewport Type", &view, views, 4)) {
            ctx.viewMode = static_cast<EEditorViewMode>(view);
        }
        int bufferView = static_cast<int>(ctx.viewportViewMode);
        const char* bufferViews[] = {"Lit", "Player Collision"};
        if (ImGui::Combo("View Mode", &bufferView, bufferViews, 2)) {
            ctx.viewportViewMode = static_cast<EEditorViewportViewMode>(bufferView);
        }
    }

    ImGui::End();
}

} // namespace leon::editor
