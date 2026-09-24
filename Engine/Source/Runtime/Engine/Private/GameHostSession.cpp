#include "GameHostSession.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include "Misc/Paths.h"
#include "GameFramework/DefaultGameMode.h"
#include "ProjectDescriptor.h"
#include <memory>
#include <nlohmann/json.hpp>


FGameHostSession::~FGameHostSession() {
    Stop();
}

void FGameHostSession::BindTravelCallbacks() {
    if (Engine == nullptr) {
        return;
    }
    Engine->GetGameInstance().SetLevelTravelFn(
        [this](UGameEngine& E, std::string_view LevelKey) { return World.GetDirector().LoadByKey(E, LevelKey); });
    Engine->GetGameInstance().SetLevelBrowserVisibleFn(
        [this](bool bVisible) { World.GetDirector().SetBrowserVisible(bVisible); });
}

bool FGameHostSession::Start(UGameEngine& InEngine, const char* InPackName, FRegisterModesFunction RegisterModes,
                            std::string_view PreferredLevelKey,
                            std::string_view PackRootOverride) {
    Stop();
    if (InPackName == nullptr || InPackName[0] == '\0') {
        std::cerr << "GameHostSession: pack name is empty\n";
        return false;
    }
    if (!InEngine.IsInitialized()) {
        std::cerr << "GameHostSession: Engine is not initialized\n";
        return false;
    }

    Engine = &InEngine;
    PackName = InPackName;

    const std::string ShaderDir = FPaths::ResolveAssetPath("assets/Shaders");
    if (!World.Initialize(InEngine, ShaderDir)) {
        std::cerr << "GameHostSession: failed to initialize WorldRuntime\n";
        Engine = nullptr;
        PackName.clear();
        return false;
    }
    bWorldInitialized = true;

    FProjectDescriptor Pack;
    if (!PackRootOverride.empty()) {
        Pack.Name = InPackName;
        Pack.RootDirectory = std::string(PackRootOverride);
        const auto Marker = std::filesystem::path(Pack.RootDirectory) / "leon.game.json";
        std::ifstream In(Marker);
        if (In.is_open()) {
            try {
                nlohmann::json Doc;
                In >> Doc;
                Pack.DefaultLevel = Doc.value("defaultLevel", "");
            } catch (...) {
            }
        }
    } else {
        Pack = FProjectDescriptor::Resolve(InPackName);
    }
    if (Pack.RootDirectory.empty()) {
        std::cerr << "GameHostSession: could not resolve pack root for '" << InPackName << "'\n";
        World.Shutdown();
        bWorldInitialized = false;
        Engine = nullptr;
        PackName.clear();
        return false;
    }
    FPaths::SetActiveContentRoot(Pack.RootDirectory);

    std::string LevelKey(PreferredLevelKey);
    if (LevelKey.empty()) {
        LevelKey = Pack.DefaultLevelKey();
    }

    if (!World.LoadPack(InEngine, Pack.RootDirectory, LevelKey)) {
        std::cerr << "GameHostSession: no levels under '" << Pack.RootDirectory << "'\n";
        std::cerr << "Expected Content/Levels/*.llev under the pack root\n";
        FPaths::SetActiveContentRoot({});
        World.Shutdown();
        bWorldInitialized = false;
        Engine = nullptr;
        PackName.clear();
        return false;
    }
    if (!LevelKey.empty()) {
        std::cout << "GameHostSession pack '" << InPackName << "' start level: " << LevelKey << '\n';
    }

    BindTravelCallbacks();

    Gameplay = FGameplayRouter{};
    Gameplay.SetDefaultMode(std::make_unique<ADefaultGameMode>());
    if (RegisterModes) {
        // Packs may SetGameInstance<T>() here before modes run.
        RegisterModes(InEngine, Gameplay);
    }
    BindTravelCallbacks();

    bActive = true;
    // Bind the GameMode for the loaded level immediately (Unreal BeginPlay).
    World.Tick(InEngine, Gameplay, 0.0f);
    return true;
}

void FGameHostSession::Tick(float DeltaTime) {
    if (!bActive || Engine == nullptr) {
        return;
    }
    World.Tick(*Engine, Gameplay, DeltaTime);
}

void FGameHostSession::HandleUiInput() {
    if (!bActive || Engine == nullptr) {
        return;
    }
    World.HandleUiInput(*Engine);
}

void FGameHostSession::DrawUi(int FramebufferWidth, int FramebufferHeight) {
    if (!bActive) {
        return;
    }
    World.DrawUi(FramebufferWidth, FramebufferHeight);
}

void FGameHostSession::Stop() {
    if (!bActive && !bWorldInitialized) {
        return;
    }

    if (Engine != nullptr) {
        if (AGameModeBase* Mode = Gameplay.GetActive()) {
            Mode->OnExit(*Engine);
        }
        // Restore base UGameInstance (pack may have swapped CoopGameInstance, etc.).
        Engine->SetGameInstance<UGameInstance>();
        Engine->GetGameInstance().SetLevelTravelFn({});
        Engine->GetGameInstance().SetLevelBrowserVisibleFn({});
        Engine->SetShaderReloadHook({});
        Engine->ClearCenterHudText();
        Engine->GetHUD().Clear();
    }

    Gameplay = FGameplayRouter{};
    if (bWorldInitialized) {
        World.Shutdown();
        bWorldInitialized = false;
    }
    FPaths::SetActiveContentRoot({});
    Engine = nullptr;
    PackName.clear();
    bActive = false;
}

