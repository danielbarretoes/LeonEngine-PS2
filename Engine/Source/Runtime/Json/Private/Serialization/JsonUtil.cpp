#include "Serialization/JsonUtil.h"

#include <fstream>
#include <iostream>

namespace leon::serialization {

glm::vec3 ReadVec3(const nlohmann::json& j, const glm::vec3& fallback) {
    if (!j.is_array() || j.size() < 3) {
        return fallback;
    }
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

bool LoadJsonFile(const std::string& path, nlohmann::json& out) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "Serialization: cannot open " << path << '\n';
        return false;
    }
    try {
        in >> out;
    } catch (const std::exception& ex) {
        std::cerr << "Serialization: JSON error in " << path << ": " << ex.what() << '\n';
        return false;
    }
    return true;
}

} // namespace leon::serialization
