#include <leon/gameplay/GameInstance.h>

#include <filesystem>
#include <iostream>
#include <leon/core/Ascii.h>
#include <leon/Engine.h>
#include <leon/level/LeonLevelFormat.h>
#include <leon/level/LevelLoader.h>

namespace leon {
namespace {

[[nodiscard]] bool TravelViaSiblingLevels(Engine& engine, std::string_view levelKey,
                                          std::string_view hintLevelPath) {
    if (levelKey.empty() || hintLevelPath.empty()) {
        return false;
    }
    namespace fs = std::filesystem;
    const fs::path hint(hintLevelPath);
    const fs::path levelsDir = hint.parent_path();
    if (levelsDir.empty()) {
        return false;
    }

    const std::string needle = AsciiToLower(levelKey);
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(levelsDir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const fs::path path = entry.path();
        if (AsciiToLower(path.extension().string()) != ".llev") {
            continue;
        }
        const std::string stem = AsciiToLower(path.stem().string());
        bool match = (stem == needle) || (AsciiToLower(path.filename().string()) == needle);
        if (!match) {
            LevelDocument doc;
            if (LoadLeonLevelFile(path.string(), doc) && AsciiToLower(doc.name) == needle) {
                match = true;
            }
        }
        if (!match) {
            continue;
        }
        return LoadLevelFile(engine, path.lexically_normal().string());
    }
    return false;
}

} // namespace

GameInstance::GameInstance() : netDriver_(std::make_unique<NetDriver>()) {}

GameInstance::~GameInstance() {
    Shutdown();
}

void GameInstance::Init() {}

void GameInstance::Shutdown() {
    CloseNetSession();
    levelTravelFn_ = {};
    levelBrowserVisibleFn_ = {};
}

bool GameInstance::HostListen(std::uint16_t port) {
    return netDriver_->StartHost(port);
}

bool GameInstance::HostDedicated(std::uint16_t port) {
    return netDriver_->StartDedicated(port);
}

bool GameInstance::Join(const std::string& address, std::uint16_t port) {
    return netDriver_->Connect(address, port);
}

void GameInstance::CloseNetSession() {
    if (netDriver_) {
        netDriver_->Shutdown();
    }
}

bool GameInstance::TravelInternal(Engine& engine, std::string_view levelKey,
                                  std::string_view hintLevelPath) {
    if (levelKey.empty()) {
        return false;
    }
    if (levelTravelFn_ && levelTravelFn_(engine, levelKey)) {
        return true;
    }
    if (TravelViaSiblingLevels(engine, levelKey, hintLevelPath)) {
        return true;
    }
    std::cerr << "GameInstance: travel failed for map '" << levelKey << "'\n";
    return false;
}

bool GameInstance::ServerTravel(Engine& engine, std::string_view mapName,
                                std::string_view hintLevelPath) {
    return TravelInternal(engine, mapName, hintLevelPath);
}

bool GameInstance::ClientTravel(Engine& engine, std::string_view mapName,
                                std::string_view hintLevelPath) {
    return TravelInternal(engine, mapName, hintLevelPath);
}

} // namespace leon
