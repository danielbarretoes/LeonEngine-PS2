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
    [[nodiscard]] bool Start(UGameEngine& InEngine, const char* InPackName, FRegisterModesFunction RegisterModes,
                             std::string_view PreferredLevelKey = {},
                             std::string_view PackRootOverride = {});

    void Tick(float DeltaTime);
    void HandleUiInput();
    void DrawUi(int FramebufferWidth, int FramebufferHeight);

    /// Exit active GameMode, shut down FWorldRuntime, restore base UGameInstance, clear content root.
    void Stop();

    [[nodiscard]] bool IsActive() const { return bActive; }
    [[nodiscard]] FGameplayRouter& Router() { return Gameplay; }
    [[nodiscard]] const FGameplayRouter& Router() const { return Gameplay; }
    [[nodiscard]] FWorldRuntime& GetWorldRuntime() { return World; }
    [[nodiscard]] const FWorldRuntime& GetWorldRuntime() const { return World; }
    [[nodiscard]] UGameEngine* GetEngine() const { return Engine; }
    [[nodiscard]] const std::string& GetPackName() const { return PackName; }

private:
    void BindTravelCallbacks();

    UGameEngine* Engine = nullptr;
    FWorldRuntime World;
    FGameplayRouter Gameplay;
    std::string PackName;
    bool bActive = false;
    bool bWorldInitialized = false;
};

