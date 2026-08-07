#include <leon/editor/EditorLevelFactory.h>

#include <filesystem>
#include <leon/core/Paths.h>
#include <leon/level/LevelLoader.h>

namespace leon::editor {

bool CreateLevelFromTemplate(ENewLevelTemplate tmpl, Engine& engine, std::string& outError) {
    outError.clear();

    const char* relative = nullptr;
    switch (tmpl) {
    case ENewLevelTemplate::Blank:
        relative = "LevelTemplates/Blank.llev";
        break;
    case ENewLevelTemplate::Starter:
        relative = "LevelTemplates/Starter.llev";
        break;
    default:
        outError = "Unknown level template.";
        return false;
    }

    const std::string path = ResolveAssetPath(relative);
    std::error_code ec;
    if (path.empty() || !std::filesystem::is_regular_file(path, ec) || ec) {
        outError = std::string("Level template not found: ") + relative;
        return false;
    }

    if (!LoadLevelFile(engine, path)) {
        outError = std::string("Failed to load level template: ") + relative;
        return false;
    }
    return true;
}

} // namespace leon::editor
