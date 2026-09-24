#pragma once

#include <functional>
#include "Engine/GameEngine.h"
#include "GameFramework/GameplayRouter.h"
#include "WorldRuntime.h"
#include <string>
#include <string_view>


/// Embeddable play host: pack resolve, FWorldRuntime, FGameplayRouter — no GLFW loop.
/// Used by shipping `FGameApplication::Run` and Editor PIE (Selected Viewport / New Window).
class FGameHostSession {
public:
    using FRegisterModesFunction = std::function<void(UGameEngine&, FGameplayRouter&)>;

    FGameHostSession() = default;
    ~FGameHostSession();

    FGameHostSession(const FGameHostSession&) = delete;
    FGameHostSession& operator=(const FGameHostSession&) = delete;

    /// Flow: content root → LoadPack → travel/browser callbacks → registerModes → first Sync via Tick.
    /// `preferredLevelKey` empty → pack `defaultLevel` (shipping). Editor PIE passes the open level key.
    /// `packRootOverride` empty → `FProjectDescriptor::Resolve`; Editor passes the open project path.
    [[nodiscard]] bool Start(UGameEngine& engine, const char* packName, FRegisterModesFunction registerModes,
                             std::string_view preferredLevelKey = {},
                             std::string_view packRootOverride = {});

    void Tick(float deltaTime);
    void HandleUiInput();
    void DrawUi(int framebufferWidth, int framebufferHeight);

    /// Exit active GameMode, shut down FWorldRuntime, restore base UGameInstance, clear content root.
    void Stop();

    [[nodiscard]] bool IsActive() const { return active_; }
    [[nodiscard]] FGameplayRouter& Router() { return gameplay_; }
    [[nodiscard]] const FGameplayRouter& Router() const { return gameplay_; }
    [[nodiscard]] FWorldRuntime& GetWorldRuntime() { return world_; }
    [[nodiscard]] const FWorldRuntime& GetWorldRuntime() const { return world_; }
    [[nodiscard]] UGameEngine* GetEngine() const { return engine_; }
    [[nodiscard]] const std::string& PackName() const { return packName_; }

private:
    void BindTravelCallbacks();

    UGameEngine* engine_ = nullptr;
    FWorldRuntime world_;
    FGameplayRouter gameplay_;
    std::string packName_;
    bool active_ = false;
    bool worldInitialized_ = false;
};

