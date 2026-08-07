#include <leon/editor/panels/ProjectSettingsPanel.h>

#include <cfloat>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <leon/editor/EditorCatalogs.h>
#include <leon/editor/EditorProject.h>
#include <leon/level/LevelCatalog.h>

namespace leon::editor {
namespace {

[[nodiscard]] std::string BasenameLabel(const std::string& path) {
    if (path.empty()) {
        return "(none)";
    }
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

/// Authoring key stored in leon.game.json (`Levels/MainMenu.llev`).
[[nodiscard]] std::string DefaultLevelAuthoringKey(const LevelEntry& entry) {
    namespace fs = std::filesystem;
    const fs::path path(entry.path);
    return (fs::path("Levels") / path.filename()).generic_string();
}

} // namespace

void ProjectSettingsPanel::Draw(EditorContext& ctx) {
    if (!ctx.showProjectSettings) {
        return;
    }
    if (!ImGui::Begin("Project Settings", &ctx.showProjectSettings)) {
        ImGui::End();
        return;
    }

    if (ctx.projectPath.empty()) {
        ImGui::TextDisabled("Open a project to edit Project Settings");
        ImGui::End();
        return;
    }

    if (descriptionProjectKey_ != ctx.projectPath) {
        descriptionProjectKey_ = ctx.projectPath;
        descriptionDraft_ = ctx.projectDescription;
    }

    if (ImGui::CollapsingHeader("Project", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Name");
        ImGui::SameLine(140.0f);
        ImGui::TextUnformatted(ctx.projectName.empty() ? "(unknown)" : ctx.projectName.c_str());

        char displayBuf[128];
        (void)std::snprintf(displayBuf, sizeof(displayBuf), "%s", ctx.projectDisplayName.c_str());
        if (ImGui::InputText("Display Name", displayBuf, sizeof(displayBuf))) {
            std::string err;
            if (EditorProjectService::WriteProjectDisplayName(ctx.projectPath, displayBuf, err)) {
                ctx.projectDisplayName = displayBuf;
            } else {
                std::cerr << "Editor: " << err << '\n';
            }
        }

        char descBuf[512];
        (void)std::snprintf(descBuf, sizeof(descBuf), "%s", descriptionDraft_.c_str());
        if (ImGui::InputTextMultiline("Description", descBuf, sizeof(descBuf),
                                      ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3.5f))) {
            descriptionDraft_ = descBuf;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string err;
            if (EditorProjectService::WriteProjectDescription(ctx.projectPath, descriptionDraft_,
                                                             err)) {
                ctx.projectDescription = descriptionDraft_;
            } else {
                std::cerr << "Editor: " << err << '\n';
            }
        }
    }

    if (ImGui::CollapsingHeader("Maps & Modes", ImGuiTreeNodeFlags_DefaultOpen)) {
        const std::string& currentDefault = ctx.projectDefaultLevel;
        const std::string preview =
            currentDefault.empty() ? std::string("(first catalog entry)") : BasenameLabel(currentDefault);
        if (ImGui::BeginCombo("Game Default Map", preview.c_str())) {
            if (ImGui::Selectable("(first catalog entry)", currentDefault.empty())) {
                std::string err;
                if (EditorProjectService::WriteProjectDefaultLevel(ctx.projectPath, {}, err)) {
                    ctx.projectDefaultLevel.clear();
                } else {
                    std::cerr << "Editor: " << err << '\n';
                }
            }
            if (ctx.catalog != nullptr) {
                for (const LevelEntry& entry : ctx.catalog->Entries()) {
                    const std::string key = DefaultLevelAuthoringKey(entry);
                    const bool selected =
                        currentDefault == key || BasenameLabel(currentDefault) == entry.name ||
                        BasenameLabel(currentDefault) ==
                            std::filesystem::path(entry.path).filename().string();
                    const std::string label = entry.name.empty() ? key : entry.name;
                    if (ImGui::Selectable(label.c_str(), selected)) {
                        std::string err;
                        if (EditorProjectService::WriteProjectDefaultLevel(ctx.projectPath, key,
                                                                          err)) {
                            ctx.projectDefaultLevel = key;
                        } else {
                            std::cerr << "Editor: " << err << '\n';
                        }
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", key.c_str());
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Shipping OpenLevel / pack startup (`defaultLevel`)");

        const std::vector<std::string> modes = ListEditorGameModes(ctx.projectPath);
        std::string currentMode = ctx.projectDefaultGameMode;
        if (currentMode.empty()) {
            currentMode = "Default";
        }
        bool currentKnown = false;
        for (const std::string& mode : modes) {
            if (mode == currentMode) {
                currentKnown = true;
                break;
            }
        }
        const std::string modePreview =
            currentKnown ? currentMode
                         : (ctx.projectDefaultGameMode.empty() ? std::string("(unset)")
                                                               : currentMode + " (custom)");
        if (ImGui::BeginCombo("Default GameMode", modePreview.c_str())) {
            if (ImGui::Selectable("(unset)", ctx.projectDefaultGameMode.empty())) {
                std::string err;
                if (EditorProjectService::WriteProjectDefaultGameMode(ctx.projectPath, {}, err)) {
                    ctx.projectDefaultGameMode.clear();
                } else {
                    std::cerr << "Editor: " << err << '\n';
                }
            }
            for (const std::string& mode : modes) {
                const bool selected = (mode == ctx.projectDefaultGameMode);
                if (ImGui::Selectable(mode.c_str(), selected)) {
                    std::string err;
                    if (EditorProjectService::WriteProjectDefaultGameMode(ctx.projectPath, mode,
                                                                         err)) {
                        ctx.projectDefaultGameMode = mode;
                    } else {
                        std::cerr << "Editor: " << err << '\n';
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Used when a level has no GameMode Override (`defaultGameMode`)");
    }

    if (ImGui::CollapsingHeader("Build", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool buildServer = ctx.projectBuildDedicatedServer;
        if (ImGui::Checkbox("Build dedicated headless server", &buildServer)) {
            std::string err;
            if (EditorProjectService::WriteProjectBuildDedicatedServer(ctx.projectPath, buildServer,
                                                                       err)) {
                ctx.projectBuildDedicatedServer = buildServer;
            } else {
                std::cerr << "Editor: " << err << '\n';
            }
        }
        ImGui::TextDisabled("Also builds leon-<Name>-server into Shipping/ (Win + Linux scripts)");
    }

    ImGui::End();
}

} // namespace leon::editor
