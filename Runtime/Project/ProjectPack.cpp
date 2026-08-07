#include <leon/runtime/ProjectPack.h>

#include <filesystem>
#include <fstream>
#include <leon/core/Paths.h>
#include <nlohmann/json.hpp>

namespace leon::runtime {

ProjectPack ProjectPack::Resolve(const char* packName) {
    ProjectPack pack;
    pack.name = packName != nullptr ? packName : "";
    if (pack.name.empty()) {
        return pack;
    }
    pack.rootDirectory = ResolveAssetPath(std::string("Projects/") + pack.name);
    if (pack.rootDirectory.empty()) {
        return pack;
    }

    const auto marker = std::filesystem::path(pack.rootDirectory) / "leon.game.json";
    std::ifstream in(marker);
    if (!in.is_open()) {
        return pack;
    }
    try {
        nlohmann::json doc;
        in >> doc;
        pack.defaultLevel = doc.value("defaultLevel", "");
    } catch (...) {
        // Keep pack root; startup falls back to first catalog entry.
    }
    return pack;
}

std::string ProjectPack::DefaultLevelKey() const {
    if (defaultLevel.empty()) {
        return {};
    }
    return std::filesystem::path(defaultLevel).stem().string();
}

} // namespace leon::runtime
