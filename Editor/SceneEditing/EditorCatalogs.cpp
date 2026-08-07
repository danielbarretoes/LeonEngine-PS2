#include <leon/editor/EditorCatalogs.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <leon/core/Paths.h>
#include <leon/level/LeonLevelFormat.h>
#include <nlohmann/json.hpp>
#include <system_error>

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

void AppendUniqueMode(std::vector<std::string>& modes, const std::string& id) {
    if (id.empty()) {
        return;
    }
    if (std::find(modes.begin(), modes.end(), id) != modes.end()) {
        return;
    }
    modes.push_back(id);
}

void AppendModesFromLeonGameJson(std::vector<std::string>& modes, const fs::path& projectPath) {
    const fs::path marker = projectPath / "leon.game.json";
    std::ifstream in(marker);
    if (!in.is_open()) {
        return;
    }
    nlohmann::json doc;
    try {
        in >> doc;
    } catch (...) {
        return;
    }
    if (doc.contains("defaultGameMode") && doc["defaultGameMode"].is_string()) {
        AppendUniqueMode(modes, doc["defaultGameMode"].get<std::string>());
    }
    if (doc.contains("gameModes") && doc["gameModes"].is_array()) {
        for (const auto& entry : doc["gameModes"]) {
            if (entry.is_string()) {
                AppendUniqueMode(modes, entry.get<std::string>());
            }
        }
    }
}

void AppendModesFromLevels(std::vector<std::string>& modes, const fs::path& projectPath) {
    const fs::path levelsDir = ProjectContentDirectory(projectPath) / "Levels";
    std::error_code ec;
    if (!fs::is_directory(levelsDir, ec)) {
        return;
    }
    for (const auto& it : fs::directory_iterator(levelsDir, ec)) {
        if (ec || !it.is_regular_file()) {
            continue;
        }
        if (it.path().extension() != ".llev") {
            continue;
        }
        LevelDocument level;
        if (!LoadLeonLevelFile(it.path().string(), level)) {
            continue;
        }
        AppendUniqueMode(modes, level.gameMode);
    }
}

template <typename EntryT>
void DedupByPath(std::vector<EntryT>& out) {
    std::sort(out.begin(), out.end(), [](const EntryT& a, const EntryT& b) {
        if (a.displayName != b.displayName) {
            return a.displayName < b.displayName;
        }
        return a.authoringPath < b.authoringPath;
    });
    out.erase(std::unique(out.begin(), out.end(),
                          [](const EntryT& a, const EntryT& b) {
                              return a.authoringPath == b.authoringPath ||
                                     (!a.absolutePath.empty() && a.absolutePath == b.absolutePath);
                          }),
              out.end());
}

void AppendMaterialsFromDir(std::vector<EditorMaterialEntry>& out, const fs::path& dir,
                            const fs::path& authoringRoot) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return;
    }
    for (const auto& it : fs::recursive_directory_iterator(dir, ec)) {
        if (ec || !it.is_regular_file()) {
            continue;
        }
        if (it.path().extension() != ".lmat") {
            continue;
        }
        EditorMaterialEntry entry;
        entry.displayName = it.path().stem().string();
        entry.absolutePath = it.path().generic_string();
        std::error_code relEc;
        const fs::path rel = fs::relative(it.path(), authoringRoot, relEc);
        if (!relEc) {
            entry.authoringPath = rel.generic_string();
        } else {
            entry.authoringPath = entry.absolutePath;
        }
        out.push_back(std::move(entry));
    }
}

void AppendHdrFromDir(std::vector<EditorSkyboxEntry>& out, const fs::path& dir,
                      const fs::path& authoringRoot) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return;
    }
    for (const auto& it : fs::recursive_directory_iterator(dir, ec)) {
        if (ec || !it.is_regular_file()) {
            continue;
        }
        std::string ext = it.path().extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext != ".hdr") {
            continue;
        }
        EditorSkyboxEntry entry;
        entry.displayName = it.path().stem().string();
        entry.absolutePath = it.path().generic_string();
        std::error_code relEc;
        const fs::path rel = fs::relative(it.path(), authoringRoot, relEc);
        if (!relEc) {
            entry.authoringPath = rel.generic_string();
        } else {
            entry.authoringPath = entry.absolutePath;
        }
        out.push_back(std::move(entry));
    }
}

void AppendEngineMaterial(std::vector<EditorMaterialEntry>& out, const char* authoringRel,
                          const char* displayName) {
    EditorMaterialEntry entry;
    entry.displayName = displayName;
    entry.authoringPath = authoringRel;
    entry.absolutePath = ResolveAssetPath(std::string("assets/") + authoringRel);
    if (entry.absolutePath.empty()) {
        entry.absolutePath = ResolveAssetPath(authoringRel);
    }
    out.push_back(std::move(entry));
}

void AppendEngineSkybox(std::vector<EditorSkyboxEntry>& out, const char* authoringRel,
                        const char* displayName) {
    EditorSkyboxEntry entry;
    entry.displayName = displayName;
    entry.authoringPath = authoringRel;
    entry.absolutePath = ResolveAssetPath(std::string("assets/") + authoringRel);
    if (entry.absolutePath.empty()) {
        entry.absolutePath = ResolveAssetPath(authoringRel);
    }
    out.push_back(std::move(entry));
}

} // namespace

std::vector<std::string> ListEditorGameModes(const std::string& projectPath) {
    // Built-in editor previews. Pack-specific ids come from leon.game.json / level overrides.
    std::vector<std::string> modes{"Default", "third-person"};
    if (!projectPath.empty()) {
        AppendModesFromLeonGameJson(modes, fs::path(projectPath));
        AppendModesFromLevels(modes, fs::path(projectPath));
    }
    return modes;
}

std::vector<EditorMaterialEntry> CollectEditorMaterials(const std::string& projectPath) {
    std::vector<EditorMaterialEntry> out;
    AppendEngineMaterial(out, "Materials/M_Default.lmat", "M_Default");
    AppendEngineMaterial(out, "Materials/M_WorldGrid.lmat", "M_WorldGrid");
    AppendEngineMaterial(out, "Materials/M_SolidMetal.lmat", "M_SolidMetal");

    if (!projectPath.empty()) {
        const fs::path content = ProjectContentDirectory(projectPath);
        AppendMaterialsFromDir(out, content / "Materials", content);
        AppendMaterialsFromDir(out, content / "assets" / "Materials", content);
        // Legacy project-root materials (pre-Content/ layout).
        if (content != fs::path(projectPath)) {
            AppendMaterialsFromDir(out, fs::path(projectPath) / "Materials", fs::path(projectPath));
        }
    }

    DedupByPath(out);
    return out;
}

std::vector<EditorSkyboxEntry> CollectEditorSkyboxes(const std::string& projectPath) {
    std::vector<EditorSkyboxEntry> out;
    AppendEngineSkybox(out, "Hdr/AutumnFieldPuresky1k.hdr", "DefaultSky");

    const std::string engineHdrDir = ResolveAssetPath("assets/Hdr");
    if (!engineHdrDir.empty()) {
        // Authoring paths relative to assets/ so level stores Hdr/foo.hdr.
        const fs::path assetsRoot = fs::path(engineHdrDir).parent_path();
        AppendHdrFromDir(out, engineHdrDir, assetsRoot);
    }

    if (!projectPath.empty()) {
        const fs::path content = ProjectContentDirectory(projectPath);
        AppendHdrFromDir(out, content / "hdr", content);
        AppendHdrFromDir(out, content / "assets" / "hdr", content);
        if (content != fs::path(projectPath)) {
            AppendHdrFromDir(out, fs::path(projectPath) / "hdr", fs::path(projectPath));
        }
    }

    DedupByPath(out);
    return out;
}

} // namespace leon::editor
