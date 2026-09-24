#include "Engine/GameInstance.h"

#include <filesystem>
#include <iostream>
#include "Misc/CString.h"
#include "Engine/GameEngine.h"
#include "Level/LeonLevelFormat.h"
#include "Level/LevelLoader.h"

namespace {

[[nodiscard]] bool TravelViaSiblingLevels(UGameEngine& engine, std::string_view levelKey,
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

    const std::string needle = FCString::ToLower(levelKey);
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(levelsDir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const fs::path path = entry.path();
        if (FCString::ToLower(path.extension().string()) != ".llev") {
            continue;
        }
        const std::string stem = FCString::ToLower(path.stem().string());
        bool match = (stem == needle) || (FCString::ToLower(path.filename().string()) == needle);
        if (!match) {
            FLevelDocument doc;
            if (LoadLeonLevelFile(path.string(), doc) && FCString::ToLower(doc.name) == needle) {
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

UGameInstance::UGameInstance() : netDriver_(std::make_unique<UNetDriver>()) {}

UGameInstance::~UGameInstance() {
    Shutdown();
}

void UGameInstance::Init() {}

void UGameInstance::Shutdown() {
    CloseNetSession();
    levelTravelFn_ = {};
    levelBrowserVisibleFn_ = {};
}

bool UGameInstance::HostListen(std::uint16_t port) {
    return netDriver_->StartHost(port);
}

bool UGameInstance::HostDedicated(std::uint16_t port) {
    return netDriver_->StartDedicated(port);
}

bool UGameInstance::Join(const std::string& address, std::uint16_t port) {
    return netDriver_->Connect(address, port);
}

void UGameInstance::CloseNetSession() {
    if (netDriver_) {
        netDriver_->Shutdown();
    }
}

bool UGameInstance::TravelInternal(UGameEngine& engine, std::string_view levelKey,
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

bool UGameInstance::ServerTravel(UGameEngine& engine, std::string_view mapName,
                                std::string_view hintLevelPath) {
    return TravelInternal(engine, mapName, hintLevelPath);
}

bool UGameInstance::ClientTravel(UGameEngine& engine, std::string_view mapName,
                                std::string_view hintLevelPath) {
    return TravelInternal(engine, mapName, hintLevelPath);
}

