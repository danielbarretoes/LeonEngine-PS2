#include <cstdint>
#include <iostream>
#include <leon/editor/LevelSaver.h>
#include <leon/level/LeonLevelFormat.h>
#include <string>
#include <vector>

namespace leon::editor {

bool saveLevelFile(const std::string& levelPath, const Level& level, const Camera& camera) {
    return SaveLeonLevelFile(levelPath, BuildLevelDocument(level, camera));
}

std::string SerializeLevelSnapshot(const Level& level, const Camera& camera) {
    const std::vector<std::uint8_t> bytes = SerializeLeonLevel(BuildLevelDocument(level, camera));
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

bool LoadLevelSnapshot(Engine& engine, const std::string& bytes, const std::string& debugName) {
    if (bytes.empty()) {
        return false;
    }
    const std::vector<std::uint8_t> raw(bytes.begin(), bytes.end());
    LevelDocument doc;
    if (!DeserializeLeonLevel(raw, doc)) {
        std::cerr << "LoadLevelSnapshot: undecodable snapshot for " << debugName << '\n';
        return false;
    }
    if (!ApplyLevelDocument(engine, doc, debugName, nullptr)) {
        std::cerr << "LoadLevelSnapshot failed for " << debugName << '\n';
        return false;
    }
    return true;
}

} // namespace leon::editor
