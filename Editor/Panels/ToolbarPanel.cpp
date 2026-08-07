#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <iostream>
#include <imgui.h>
#include <imgui_internal.h>
#include <leon/Engine.h>
#include <leon/editor/EditorToast.h>
#include <leon/editor/panels/ToolbarPanel.h>
#include <leon/gameplay/GameMode.h>
#include <leon/gameplay/World.h>
#include <leon/net/NetProtocol.h>
#include <string>

namespace leon::editor {
namespace {

const char* PlayModeLabel(EEditorPlayMode mode) {
    switch (mode) {
    case EEditorPlayMode::NewEditorWindow:
        return "New Window";
    case EEditorPlayMode::SelectedViewport:
    default:
        return "Selected Viewport";
    }
}

const char* PlayNetModeLabel(EEditorPlayNetMode mode) {
    switch (mode) {
    case EEditorPlayNetMode::ListenServer:
        return "Listen Server";
    case EEditorPlayNetMode::Client:
        return "Client";
    case EEditorPlayNetMode::Standalone:
    default:
        return "Standalone";
    }
}

void BakeNavigationPaths(EditorContext& ctx) {
    if (ctx.level == nullptr) {
        return;
    }

    // Flow: Editor Preview Paths (in-memory NavMesh bake — not written to disk)
    // 1. Estimate floor / walk bounds from Level
    // 2. Prefer live PIE World nav; otherwise scratch World + PhysScene
    // 3. RegisterBodiesFromLevel → SyncFromLevel → NavigationSystem::BuildFromLevel
    const float floorY = leon::GameMode::EstimateFloorY(*ctx.level);
    const float walkBounds = leon::GameMode::EstimateWalkBounds(*ctx.level);

    auto runBake = [&](leon::World& world) {
        world.RegisterBodiesFromLevel(*ctx.level);
        world.GetPhysicsScene().SyncFromLevel(*ctx.level);
        leon::NavigationSystem& nav = world.GetNavigationSystem();
        nav.SetCellSize(0.5f);
        nav.SetAgentRadius(0.45f);
        nav.BuildFromLevel(*ctx.level, world.GetPhysicsScene(), floorY, walkBounds);
        const std::string msg =
            "NavMesh preview (not saved) walkable=" + std::to_string(nav.WalkableCellCount()) +
            " blockers=" + std::to_string(nav.BlockerCount()) +
            " cell=" + std::to_string(nav.CellSize());
        std::cout << "Editor: " << msg << '\n';
        EditorToast(msg, EEditorToastKind::Success, 4.0f);
        if (ctx.engine != nullptr) {
            ctx.engine->AddOnScreenDebugMessage(msg, 4.0f, {0.45f, 0.9f, 0.55f});
        }
    };

    if (ctx.world != nullptr && ctx.piePlaying) {
        runBake(*ctx.world);
    } else {
        leon::World scratch;
        runBake(scratch);
    }
}

} // namespace

void ToolbarPanel::Draw(EditorContext& ctx) {
    ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 36.0f), ImVec2(FLT_MAX, 64.0f));
    if (!ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    auto modeButton = [&](const char* label, EGizmoOperation op) {
        const bool active = ctx.gizmoOp == op;
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button(label)) {
            ctx.gizmoOp = op;
        }
        if (active) {
            ImGui::PopStyleColor();
        }
    };

    modeButton("Translate (W)", EGizmoOperation::Translate);
    ImGui::SameLine();
    modeButton("Rotate (E)", EGizmoOperation::Rotate);
    ImGui::SameLine();
    modeButton("Scale (R)", EGizmoOperation::Scale);

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    if (ImGui::Button(ctx.gizmoSpace == EGizmoSpace::Local ? "Local" : "World")) {
        ctx.gizmoSpace =
            ctx.gizmoSpace == EGizmoSpace::Local ? EGizmoSpace::World : EGizmoSpace::Local;
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    DrawPlayControls(ctx);

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    if (ImGui::Button("Build Lights")) {
        ctx.requestBuildLights = true;
    }
    if (!ctx.buildLightsStatus.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", ctx.buildLightsStatus.c_str());
    }

    ImGui::SameLine();
    if (ImGui::Button("Build Paths")) {
        BakeNavigationPaths(ctx);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Preview-only NavMesh bake (not saved to disk). Use F3 in PIE/shipping for debug draw.");
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    {
        const bool active = ctx.showStats;
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button("Stats")) {
            ctx.showStats = !ctx.showStats;
        }
        if (active) {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toggle FPS / frame time overlay");
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|  %s%s", ctx.levelPath.empty() ? "(unsaved)" : ctx.levelPath.c_str(),
                        ctx.dirty ? " *" : "");

    ImGui::End();
}

void ToolbarPanel::DrawPlayControls(EditorContext& ctx) {
    if (!ctx.piePlaying) {
        ImGui::BeginGroup();
        if (ImGui::Button("Play", ImVec2(64.0f, 0.0f))) {
            ctx.requestPieStart = true;
        }
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::SetNextItemWidth(132.0f);
        if (ImGui::BeginCombo("##PlayMode", PlayModeLabel(ctx.playMode),
                              ImGuiComboFlags_HeightSmall)) {
            if (ImGui::Selectable("Selected Viewport (PIE)",
                                  ctx.playMode == EEditorPlayMode::SelectedViewport)) {
                ctx.playMode = EEditorPlayMode::SelectedViewport;
            }
            if (ImGui::Selectable("New Editor Window",
                                  ctx.playMode == EEditorPlayMode::NewEditorWindow)) {
                ctx.playMode = EEditorPlayMode::NewEditorWindow;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::SetNextItemWidth(44.0f);
        int players = ctx.pieNumberOfPlayers;
        if (ImGui::DragInt("##PiePlayers", &players, 0.08f, 1, leon::net::kMaxPlayers, "%d")) {
            ctx.pieNumberOfPlayers = std::clamp(players, 1, leon::net::kMaxPlayers);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Number of Players (Unreal Play settings)\n"
                "Listen Server / Client with N>1: exactly N Shipping windows\n"
                "(editor stays in edit mode; Stop closes them)");
        }
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::SetNextItemWidth(118.0f);
        if (ImGui::BeginCombo("##PieNetMode", PlayNetModeLabel(ctx.pieNetMode),
                              ImGuiComboFlags_HeightSmall)) {
            if (ImGui::Selectable("Play Standalone",
                                  ctx.pieNetMode == EEditorPlayNetMode::Standalone)) {
                ctx.pieNetMode = EEditorPlayNetMode::Standalone;
            }
            if (ImGui::Selectable("Play As Listen Server",
                                  ctx.pieNetMode == EEditorPlayNetMode::ListenServer)) {
                ctx.pieNetMode = EEditorPlayNetMode::ListenServer;
            }
            if (ImGui::Selectable("Play As Client",
                                  ctx.pieNetMode == EEditorPlayNetMode::Client)) {
                ctx.pieNetMode = EEditorPlayNetMode::Client;
            }
            ImGui::EndCombo();
        }
        if (ctx.pieNetMode == EEditorPlayNetMode::Client) {
            ImGui::SameLine(0.0f, 2.0f);
            ImGui::SetNextItemWidth(110.0f);
            char addr[64];
            (void)std::snprintf(addr, sizeof(addr), "%s", ctx.pieClientAddress.c_str());
            if (ImGui::InputTextWithHint("##PieJoinAddr", "127.0.0.1", addr, sizeof(addr))) {
                ctx.pieClientAddress = addr;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Client join address (LAN / localhost)");
            }
        }
        ImGui::EndGroup();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip(
                "Play In Editor — Viewport/Window, Number of Players, Net Mode\n"
                "(Standalone / Listen Server / Client)");
        }
        return;
    }

    if (ImGui::Button(ctx.piePaused ? "Resume" : "Pause", ImVec2(64.0f, 0.0f))) {
        ctx.requestPiePauseToggle = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(64.0f, 0.0f))) {
        ctx.requestPieStop = true;
    }
    ImGui::SameLine();
    ImGui::TextColored(ctx.piePaused ? ImVec4(1.0f, 0.75f, 0.2f, 1.0f)
                                     : ImVec4(0.35f, 0.85f, 0.45f, 1.0f),
                       ctx.piePaused ? "PAUSED" : "PLAYING");
    ImGui::SameLine();
    ImGui::TextDisabled("%s x%d", PlayNetModeLabel(ctx.pieNetMode), ctx.pieNumberOfPlayers);
}

} // namespace leon::editor
