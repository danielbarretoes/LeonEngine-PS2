#pragma once

#include <functional>
#include <leon/Engine.h>
#include <leon/gameplay/GameplayRouter.h>
#include <leon/runtime/WorldRuntime.h>
#include <string>
#include <string_view>

namespace leon::runtime {

/// Embeddable play host: pack resolve, WorldRuntime, GameplayRouter — no GLFW loop.
/// Used by shipping `GameApplication::Run` and Editor PIE (Selected Viewport / New Window).
class GameHostSession {
public:
    using RegisterModesFn = std::function<void(Engine&, GameplayRouter&)>;

    GameHostSession() = default;
    ~GameHostSession();

    GameHostSession(const GameHostSession&) = delete;
    GameHostSession& operator=(const GameHostSession&) = delete;

    /// Flow: content root → LoadPack → travel/browser callbacks → registerModes → first Sync via Tick.
    /// `preferredLevelKey` empty → pack `defaultLevel` (shipping). Editor PIE passes the open level key.
    /// `packRootOverride` empty → `ProjectPack::Resolve`; Editor passes the open project path.
    [[nodiscard]] bool Start(Engine& engine, const char* packName, RegisterModesFn registerModes,
                             std::string_view preferredLevelKey = {},
                             std::string_view packRootOverride = {});

    void Tick(float deltaTime);
    void HandleUiInput();
    void DrawUi(int framebufferWidth, int framebufferHeight);

    /// Exit active GameMode, shut down WorldRuntime, restore base GameInstance, clear content root.
    void Stop();

    [[nodiscard]] bool IsActive() const { return active_; }
    [[nodiscard]] GameplayRouter& Router() { return gameplay_; }
    [[nodiscard]] const GameplayRouter& Router() const { return gameplay_; }
    [[nodiscard]] WorldRuntime& World() { return world_; }
    [[nodiscard]] const WorldRuntime& World() const { return world_; }
    [[nodiscard]] Engine* GetEngine() const { return engine_; }
    [[nodiscard]] const std::string& PackName() const { return packName_; }

private:
    void BindTravelCallbacks();

    Engine* engine_ = nullptr;
    WorldRuntime world_;
    GameplayRouter gameplay_;
    std::string packName_;
    bool active_ = false;
    bool worldInitialized_ = false;
};

} // namespace leon::runtime
