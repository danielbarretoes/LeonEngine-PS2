#pragma once

#include <leon/editor/EditorContext.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/EditorLayout.h>
#include <leon/editor/EditorProject.h>
#include <leon/editor/EditorViewportTarget.h>
#include <leon/editor/panels/WelcomePanel.h>
#include <leon/Engine.h>
#include <leon/core/Camera.h>
#include <leon/core/Window.h>
#include <leon/gameplay/GameMode.h>
#include <leon/gameplay/World.h>
#include <leon/level/LevelCatalog.h>
#include <leon/runtime/GameHostSession.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace leon::editor {

/// Unreal-like editor host: owns the Dear ImGui frame loop (not `Engine::run`).
/// Starts on the welcome hub; after a project is opened, loads levels via catalog.
class EditorApp {
public:
    [[nodiscard]] bool Initialize(leon::Engine& engine);
    void Shutdown();
    void Run(leon::Engine& engine);

private:
    void BeginImGuiFrame();
    void EndImGuiFrame();
    void UpdateWindowTitle(leon::Engine& engine);
    void StartPie(leon::Engine& engine);
    void StopPie(leon::Engine& engine);
    /// Apply Unreal Play Net Mode (Standalone / Listen Server / Client) after Pie OnEnter.
    void ApplyPieNetMode(leon::Engine& engine);
    /// Spawn extra Shipping pack processes for Number of Players > 1 (Listen Server / Client).
    void SpawnPieExtraInstances();
    /// After Listen Server host binds :7777, launch pending `--join` Shipping clients (non-blocking).
    void TickPendingPieClientSpawns();
    void TerminatePieSpawnedProcesses();
    void RenderPieWindow(leon::Engine& engine);
    void DestroyPiePresentResources();
    void PresentPieColorTexture(unsigned int colorTexture, int srcW, int srcH, int dstW,
                                int dstH);
    [[nodiscard]] bool OpenProject(leon::Engine& engine, const std::string& projectPath);
    void CloseProject(leon::Engine& engine);
    void SaveEditorCamera(const leon::Camera& camera);
    void RestoreEditorCamera(leon::Camera& camera) const;

    EditorContext ctx_;
    EditorLayout layout_;
    WelcomePanel welcome_;
    EditorProjectService projects_;
    EditorHistory history_;
    leon::World world_;
    leon::LevelCatalog catalog_;
    std::unique_ptr<leon::GameMode> pieMode_;
    /// Pack Runtime host (Unreal-like PIE). Mutually exclusive with `pieMode_` for pack play.
    leon::runtime::GameHostSession pieSession_;
    bool pieUsesHostSession_ = false;
    /// Level path to reload after pack PIE (travel may replace engine level).
    std::string pieRestoreLevelPath_;
    leon::Window pieWindow_;
    /// Scene rendered on the editor GL context; presented to the play window (shared texture).
    EditorViewportTarget pieTarget_;
    unsigned int piePresentFbo_ = 0;
    unsigned int piePresentAttachedTex_ = 0;
    leon::Camera editorCameraBackup_{};
    /// Stable storage for `ImGuiIO::IniFilename` (must outlive ImGui).
    std::string imguiIniPath_;
    bool imguiReady_ = false;
    bool escapeWasDown_ = false;
    bool projectOpen_ = false;
    bool editorCameraSaved_ = false;
    /// OS PIDs of Shipping clients launched for multi-player PIE (terminated on Stop).
    std::vector<std::uint32_t> pieSpawnedPids_;
    /// Listen Server / Client with NumberOfPlayers > 1: only Shipping windows (no editor Pie pawn).
    bool pieShippingSessionOnly_ = false;
    /// Async: wait for Shipping --listen to bind, then spawn N clients (avoids freezing Play).
    bool piePendingClientSpawn_ = false;
    bool piePendingHostPortSeen_ = false;
    int piePendingClientCount_ = 0;
    std::string piePendingClientExe_;
    std::string piePendingJoinAddr_;
    std::string piePendingPlayMap_;
    float piePendingClientWaitSeconds_ = 0.0f;
    float piePendingHostSettleSeconds_ = 0.0f;
};

} // namespace leon::editor
