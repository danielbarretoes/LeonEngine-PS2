#pragma once

#include <glm/vec3.hpp>

#include <functional>
#include "Camera/Camera.h"
#include "GameFramework/InputMapping.h"
#include "GameFramework/PlayInputTarget.h"
#include "Window.h"
#include "AudioDevice.h"
#include "Debug/DebugOverlay.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Renderer.h"
#include "ResourceCache.h"
#include "GameFramework/HUD.h"
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace leon {

/// Top-level runtime: GLFW window, main loop, orbit-camera input, FPS overlay,
/// GameInstance, and a Level/ResourceCache filled by LevelDirector (or the app).
class Engine {
public:
    using UpdateCallback = std::function<void(float deltaTime)>;
    /// Runs after PollEvents, before camera/input handling (level UI, etc.).
    using PreInputCallback = std::function<void()>;
    /// Runs after the 3D + stats HUD pass (level browser chrome, etc.).
    using PostRenderCallback = std::function<void(int fbWidth, int fbHeight)>;

    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool Initialize(int width, int height, const char* title);
    /// No GLFW / OpenGL — CPU meshes only. For `leon-server`.
    bool InitializeHeadless();
    void Shutdown();

    /// Main loop. Optional hooks: pre-input (UI), per-frame update, post-render overlays.
    void Run(const UpdateCallback& onUpdate = {}, const PreInputCallback& onPreInput = {},
             const PostRenderCallback& onPostRender = {});
    /// Fixed-timestep simulation loop (no render / swap).
    void RunHeadless(const UpdateCallback& onUpdate, float tickHz = 60.0f);

    /// Editor PIE / custom loops: same audio listener + device tick as `Run`.
    void TickPlayAudio();
    /// Editor PIE / custom loops: HUD widget tick + on-screen messages + optional F4 stats.
    void TickPlayHud(float deltaTime);
    /// Editor PIE / custom loops: paint HUD widgets + DebugOverlay (call after DrawScene).
    void PaintHudAndOverlay(int framebufferWidth, int framebufferHeight);

    void RequestQuit() { running_ = false; }
    [[nodiscard]] bool IsHeadless() const { return headless_; }
    [[nodiscard]] bool IsRunning() const { return running_; }

    [[nodiscard]] Level& GetLevel() { return level_; }
    [[nodiscard]] const Level& GetLevel() const { return level_; }
    [[nodiscard]] ResourceCache& GetResources() { return resources_; }
    [[nodiscard]] const ResourceCache& GetResources() const { return resources_; }
    [[nodiscard]] Camera& GetCamera() { return camera_; }
    [[nodiscard]] const Camera& GetCamera() const { return camera_; }
    [[nodiscard]] Window& GetWindow() { return window_; }
    [[nodiscard]] const Window& GetWindow() const { return window_; }
    [[nodiscard]] PlayerInput& GetInput() { return playerInput_; }
    [[nodiscard]] const PlayerInput& GetInput() const { return playerInput_; }
    [[nodiscard]] Renderer& GetRenderer() { return renderer_; }
    [[nodiscard]] const Renderer& GetRenderer() const { return renderer_; }
    [[nodiscard]] AudioDevice& GetAudioDevice() { return audioDevice_; }
    [[nodiscard]] const AudioDevice& GetAudioDevice() const { return audioDevice_; }
    [[nodiscard]] bool IsInitialized() const { return initialized_; }

    [[nodiscard]] GameInstance& GetGameInstance() { return *gameInstance_; }
    [[nodiscard]] const GameInstance& GetGameInstance() const { return *gameInstance_; }

    template <typename T, typename... Args>
    T* SetGameInstance(Args&&... args) {
        static_assert(std::is_base_of_v<GameInstance, T>, "T must derive from leon::GameInstance");
        // Packs call SetGameInstance after Runtime wires travel/browser callbacks — keep them.
        GameInstance::LevelTravelFn travelFn;
        GameInstance::LevelBrowserVisibleFn browserFn;
        if (gameInstance_) {
            travelFn = gameInstance_->TakeLevelTravelFn();
            browserFn = gameInstance_->TakeLevelBrowserVisibleFn();
            if (initialized_) {
                gameInstance_->Shutdown();
            }
        }
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = owned.get();
        gameInstance_ = std::move(owned);
        if (travelFn) {
            gameInstance_->SetLevelTravelFn(std::move(travelFn));
        }
        if (browserFn) {
            gameInstance_->SetLevelBrowserVisibleFn(std::move(browserFn));
        }
        if (initialized_) {
            gameInstance_->Init();
        }
        return raw;
    }

    /// When true, mouse look / orbit are paused (level browser chrome, etc.).
    void SetSuppressCameraDrag(bool suppress) { suppressCameraDrag_ = suppress; }
    [[nodiscard]] bool IsCameraDragSuppressed() const { return suppressCameraDrag_; }

    /// Capture + hide OS cursor for continuous mouse look (enabled by default for all levels).
    void SetCursorCaptured(bool captured);
    [[nodiscard]] bool IsCursorCaptured() const;

    /// Optional secondary window for PIE "New Window" input / cursor capture.
    /// Prefer `GetPlayInputTarget()` when configuring multiple fields; these remain the
    /// Unreal-like convenience API used by GameMode / PlayerController.
    void SetPlayInputWindow(Window* window);
    [[nodiscard]] Window& GetPlayInputWindow();
    [[nodiscard]] const Window& GetPlayInputWindow() const;

    /// Grouped PIE / multi-window play input state (window override + mouse-look gate).
    [[nodiscard]] PlayInputTarget& GetPlayInputTarget() { return playInputTarget_; }
    [[nodiscard]] const PlayInputTarget& GetPlayInputTarget() const { return playInputTarget_; }

    /// Editor PIE: when cursor is not OS-captured (Selected Viewport), mouse look only applies
    /// while this is true (typically Viewport hovered / play window focused).
    void SetPlayMouseLookActive(bool active) { playInputTarget_.SetMouseLookActive(active); }
    [[nodiscard]] bool IsPlayMouseLookActive() const { return playInputTarget_.IsMouseLookActive(); }

    /// When false, WASD/arrows do not tumble the orbit camera (gameplay may use them).
    void SetKeyboardOrbitEnabled(bool enabled) { keyboardOrbitEnabled_ = enabled; }

    /// When false, Engine mouse orbit + scroll→camera zoom are off (packs may drive SpringArm).
    void SetOrbitMouseEnabled(bool enabled) { orbitMouseEnabled_ = enabled; }

    /// F2 collision volumes debug (Engine tool flag — not owned by the forward Renderer).
    void SetCollisionDebugEnabled(bool enabled) { collisionDebugEnabled_ = enabled; }
    void ToggleCollisionDebug() { collisionDebugEnabled_ = !collisionDebugEnabled_; }
    [[nodiscard]] bool IsCollisionDebugEnabled() const { return collisionDebugEnabled_; }

    /// F3 NavMesh grid debug (walkable / blocked cells).
    void SetNavMeshDebugEnabled(bool enabled) { navMeshDebugEnabled_ = enabled; }
    void ToggleNavMeshDebug() { navMeshDebugEnabled_ = !navMeshDebugEnabled_; }
    [[nodiscard]] bool IsNavMeshDebugEnabled() const { return navMeshDebugEnabled_; }

    /// Consume accumulated mouse-wheel Y this frame (GLFW units). Cleared after return.
    /// When orbit mouse is enabled, Engine applies scroll to Orbit distance in handleInput first.
    [[nodiscard]] float ConsumeScrollY();

    /// Unreal-like Print String / AddOnScreenDebugMessage (top-left console; default red).
    void AddOnScreenDebugMessage(std::string message, float displaySeconds = 2.0f,
                                 const glm::vec3& color = {1.0f, 0.0f, 0.0f});

    /// Persistent top-center HUD line (cleared when empty). Packs update each Tick.
    void SetCenterHudText(std::string text) { centerHudText_ = std::move(text); }
    void ClearCenterHudText() { centerHudText_.clear(); }

    /// Runtime FPS / RAM / TRI overlay (F4). Off by default so Shipping matches Viewport / PIE.
    void SetHudStatsVisible(bool visible);
    [[nodiscard]] bool IsHudStatsVisible() const { return showHudStats_; }

    /// Unreal-like AHUD (UserWidgets / crosshair, etc.).
    [[nodiscard]] HUD& GetHUD() { return hud_; }
    [[nodiscard]] const HUD& GetHUD() const { return hud_; }

    /// Optional extra shader reload (level chrome, etc.) merged into F5 / auto-reload.
    using ShaderReloadHook = std::function<EShaderReloadResult(bool force)>;
    void SetShaderReloadHook(ShaderReloadHook hook) { shaderReloadHook_ = std::move(hook); }

private:
    [[nodiscard]] EShaderReloadResult reloadAllShaders(bool force);
    void handleInput(float deltaTime);
    void render(const PostRenderCallback& onPostRender);
    void updateHudStats(float deltaTime);

    Window window_;
    PlayInputTarget playInputTarget_;
    PlayerInput playerInput_;
    Renderer renderer_;
    DebugOverlay overlay_;
    HUD hud_;
    AudioDevice audioDevice_;
    Camera camera_;
    Level level_;
    ResourceCache resources_;
    std::unique_ptr<GameInstance> gameInstance_;

    bool running_ = false;
    bool initialized_ = false;
    bool headless_ = false;
    bool suppressCameraDrag_ = false;
    bool keyboardOrbitEnabled_ = true;
    bool orbitMouseEnabled_ = true;
    float pendingScrollY_ = 0.0f;
    std::string centerHudText_;

    bool mouseLookSampleValid_ = false;
    bool debugKeyWasDown_ = false;
    bool collisionDebugEnabled_ = false;
    bool collisionDebugKeyWasDown_ = false;
    bool navMeshDebugEnabled_ = false;
    bool navMeshDebugKeyWasDown_ = false;
    bool reloadKeyWasDown_ = false;
    bool showHudStats_ = false;
    bool hudStatsKeyWasDown_ = false;
    ShaderReloadHook shaderReloadHook_;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;

    int lastFbWidth_ = 0;
    int lastFbHeight_ = 0;

    float fpsAccumTime_ = 0.0f;
    int fpsAccumFrames_ = 0;
    float displayFps_ = 0.0f;
    float displayMs_ = 0.0f;
};

} // namespace leon
