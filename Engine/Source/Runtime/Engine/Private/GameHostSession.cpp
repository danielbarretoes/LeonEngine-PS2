#include "GameHostSession.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include "Misc/Paths.h"
#include "GameFramework/DefaultGameMode.h"
#include "ProjectPack.h"
#include <memory>
#include <nlohmann/json.hpp>

namespace leon::runtime {

GameHostSession::~GameHostSession() {
    Stop();
}

void GameHostSession::BindTravelCallbacks() {
    if (engine_ == nullptr) {
        return;
    }
    engine_->GetGameInstance().SetLevelTravelFn(
        [this](Engine& e, std::string_view levelKey) { return world_.Director().LoadByKey(e, levelKey); });
    engine_->GetGameInstance().SetLevelBrowserVisibleFn(
        [this](bool visible) { world_.Director().SetBrowserVisible(visible); });
}

bool GameHostSession::Start(Engine& engine, const char* packName, RegisterModesFn registerModes,
                            std::string_view preferredLevelKey,
                            std::string_view packRootOverride) {
    Stop();
    if (packName == nullptr || packName[0] == '\0') {
        std::cerr << "GameHostSession: pack name is empty\n";
        return false;
    }
    if (!engine.IsInitialized()) {
        std::cerr << "GameHostSession: Engine is not initialized\n";
        return false;
    }

    engine_ = &engine;
    packName_ = packName;

    const std::string shaderDir = ResolveAssetPath("assets/Shaders");
    if (!world_.Initialize(engine, shaderDir)) {
        std::cerr << "GameHostSession: failed to initialize WorldRuntime\n";
        engine_ = nullptr;
        packName_.clear();
        return false;
    }
    worldInitialized_ = true;

    ProjectPack pack;
    if (!packRootOverride.empty()) {
        pack.name = packName;
        pack.rootDirectory = std::string(packRootOverride);
        const auto marker = std::filesystem::path(pack.rootDirectory) / "leon.game.json";
        std::ifstream in(marker);
        if (in.is_open()) {
            try {
                nlohmann::json doc;
                in >> doc;
                pack.defaultLevel = doc.value("defaultLevel", "");
            } catch (...) {
            }
        }
    } else {
        pack = ProjectPack::Resolve(packName);
    }
    if (pack.rootDirectory.empty()) {
        std::cerr << "GameHostSession: could not resolve pack root for '" << packName << "'\n";
        world_.Shutdown();
        worldInitialized_ = false;
        engine_ = nullptr;
        packName_.clear();
        return false;
    }
    SetActiveContentRoot(pack.rootDirectory);

    std::string levelKey(preferredLevelKey);
    if (levelKey.empty()) {
        levelKey = pack.DefaultLevelKey();
    }

    if (!world_.LoadPack(engine, pack.rootDirectory, levelKey)) {
        std::cerr << "GameHostSession: no levels under '" << pack.rootDirectory << "'\n";
        std::cerr << "Expected Content/Levels/*.llev under the pack root\n";
        SetActiveContentRoot({});
        world_.Shutdown();
        worldInitialized_ = false;
        engine_ = nullptr;
        packName_.clear();
        return false;
    }
    if (!levelKey.empty()) {
        std::cout << "GameHostSession pack '" << packName << "' start level: " << levelKey << '\n';
    }

    BindTravelCallbacks();

    gameplay_ = GameplayRouter{};
    gameplay_.SetDefaultMode(std::make_unique<DefaultGameMode>());
    if (registerModes) {
        // Packs may SetGameInstance<T>() here before modes run.
        registerModes(engine, gameplay_);
    }
    BindTravelCallbacks();

    active_ = true;
    // Bind the GameMode for the loaded level immediately (Unreal BeginPlay).
    world_.Tick(engine, gameplay_, 0.0f);
    return true;
}

void GameHostSession::Tick(float deltaTime) {
    if (!active_ || engine_ == nullptr) {
        return;
    }
    world_.Tick(*engine_, gameplay_, deltaTime);
}

void GameHostSession::HandleUiInput() {
    if (!active_ || engine_ == nullptr) {
        return;
    }
    world_.HandleUiInput(*engine_);
}

void GameHostSession::DrawUi(int framebufferWidth, int framebufferHeight) {
    if (!active_) {
        return;
    }
    world_.DrawUi(framebufferWidth, framebufferHeight);
}

void GameHostSession::Stop() {
    if (!active_ && !worldInitialized_) {
        return;
    }

    if (engine_ != nullptr) {
        if (GameMode* mode = gameplay_.GetActive()) {
            mode->OnExit(*engine_);
        }
        // Restore base GameInstance (pack may have swapped CoopGameInstance, etc.).
        engine_->SetGameInstance<GameInstance>();
        engine_->GetGameInstance().SetLevelTravelFn({});
        engine_->GetGameInstance().SetLevelBrowserVisibleFn({});
        engine_->SetShaderReloadHook({});
        engine_->ClearCenterHudText();
        engine_->GetHUD().Clear();
    }

    gameplay_ = GameplayRouter{};
    if (worldInitialized_) {
        world_.Shutdown();
        worldInitialized_ = false;
    }
    SetActiveContentRoot({});
    engine_ = nullptr;
    packName_.clear();
    active_ = false;
}

} // namespace leon::runtime
