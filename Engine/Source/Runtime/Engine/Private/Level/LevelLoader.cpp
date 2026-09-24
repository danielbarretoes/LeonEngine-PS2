#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include "Validation/ContentValidator.h"
#include "Engine/Level.h"
#include "Level/LevelLoader.h"
#include "Level/LeonLevelFormat.h"
#include <string>

namespace {

[[nodiscard]] bool HasLeonLevelExtension(const std::string& path) {
    std::string extension = std::filesystem::path(path).extension().string();
    for (char& c : extension) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return extension == kLeonLevelExtension;
}

} // namespace

void ApplyFitHeight(UStaticMeshComponent& object, float fitHeight) {
    if (object.mesh == nullptr || fitHeight <= 0.0f) {
        return;
    }

    // Existing position is kept as an offset after auto scale / ground align.
    const glm::vec3 positionOffset = object.transform.Position;

    const glm::vec3 mn = object.mesh->GetLocalMin();
    const glm::vec3 mx = object.mesh->GetLocalMax();
    const glm::vec3 extents = mx - mn;
    const float height = std::max(extents.y, 0.001f);
    const float scale = fitHeight / height;
    const glm::vec3 center = (mn + mx) * 0.5f;

    object.transform.Scale = {scale, scale, scale};
    constexpr float kGroundEpsilon = 0.008f;
    const glm::vec3 grounded{(-center.x) * scale, ((-mn.y) * scale) + kGroundEpsilon,
                             (-center.z) * scale};
    object.transform.Position = grounded + positionOffset;
}

bool LoadLevelFile(UGameEngine& engine, const std::string& levelPath, FLevelAnimation* outAnim) {
    if (!HasLeonLevelExtension(levelPath)) {
        std::cerr << "LevelLoader: '" << levelPath << "' is not a Leon Level -- expected '"
                  << kLeonLevelExtension << "'\n";
        return false;
    }

    FLevelDocument doc;
    if (!LoadLeonLevelFile(levelPath, doc)) {
        return false;
    }

    FValidationReport report = ValidateLevelDocument(doc, levelPath);
    report.logToStderr();
    if (!report.ok()) {
        std::cerr << "LevelLoader: rejecting '" << levelPath << "' (validation failed)\n";
        return false;
    }

    return ApplyLevelDocument(engine, doc, levelPath, outAnim);
}

