#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include "Misc/CString.h"
#include "Misc/Paths.h"
#include "Level/LevelCatalog.h"
#include "Level/LeonLevelFormat.h"
#include <string_view>

namespace {

bool isLeonLevelFile(const std::filesystem::directory_entry& entry) {
    if (!entry.is_regular_file()) {
        return false;
    }
    std::string extension = entry.path().extension().string();
    for (char& c : extension) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return extension == kLeonLevelExtension;
}

void appendLevelsFromDirectory(std::vector<FLevelEntry>& out, const std::filesystem::path& levelsDir,
                               const std::string& packName) {
    std::error_code ec;
    if (!std::filesystem::is_directory(levelsDir, ec) || ec) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(levelsDir, ec)) {
        if (ec) {
            break;
        }
        if (!isLeonLevelFile(entry)) {
            continue;
        }

        FLevelEntry item;
        item.path = entry.path().lexically_normal().string();
        item.name = entry.path().stem().string();
        item.pack = packName;

        // Display name + GameMode Override are read once at catalog scan (FGameplayRouter uses it).
        FLevelDocument doc;
        if (LoadLeonLevelFile(item.path, doc)) {
            if (!doc.name.empty()) {
                item.name = doc.name;
            }
            item.gameMode = doc.gameMode;
        }

        out.push_back(std::move(item));
    }
}

bool directoryHasLeonLevels(const std::filesystem::path& levelsDir) {
    std::error_code ec;
    if (!std::filesystem::is_directory(levelsDir, ec) || ec) {
        return false;
    }
    for (const auto& entry : std::filesystem::directory_iterator(levelsDir, ec)) {
        if (ec) {
            break;
        }
        if (isLeonLevelFile(entry)) {
            return true;
        }
    }
    return false;
}

} // namespace

bool FLevelCatalog::Scan(const std::string& directory) {
    entries_.clear();
    directory_ = directory;

    std::error_code ec;
    const std::filesystem::path dir(directory);
    if (!std::filesystem::is_directory(dir, ec) || ec) {
        std::cerr << "LevelCatalog: not a directory: " << directory << '\n';
        return false;
    }

    appendLevelsFromDirectory(entries_, dir, {});
    std::sort(entries_.begin(), entries_.end(),
              [](const FLevelEntry& a, const FLevelEntry& b) { return a.path < b.path; });

    std::cout << "LevelCatalog: " << entries_.size() << " Level(s) in " << directory << '\n';
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        std::cout << "  [" << i << "] " << entries_[i].name << " (" << entries_[i].path << ")\n";
    }
    return !entries_.empty();
}

bool FLevelCatalog::ScanPack(const std::string& packDirectory) {
    entries_.clear();
    directory_ = packDirectory;

    std::error_code ec;
    const std::filesystem::path packDir(packDirectory);
    if (!std::filesystem::is_directory(packDir, ec) || ec) {
        std::cerr << "LevelCatalog: pack is not a directory: " << packDirectory << '\n';
        return false;
    }

    const std::string packName = packDir.filename().string();
    // Unreal-like: `<pack>/Content/Levels` (via FPaths::ProjectContentDir).
    const std::filesystem::path levelsDir = FPaths::ProjectContentDir(packDir) / "Levels";

    if (directoryHasLeonLevels(levelsDir)) {
        appendLevelsFromDirectory(entries_, levelsDir, packName);
    }

    std::sort(entries_.begin(), entries_.end(),
              [](const FLevelEntry& a, const FLevelEntry& b) { return a.path < b.path; });

    std::cout << "LevelCatalog: " << entries_.size() << " Level(s) in pack " << packName << '\n';
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const FLevelEntry& e = entries_[i];
        std::cout << "  [" << i << "] " << e.pack << "/" << e.name << " (" << e.path << ")\n";
    }
    return !entries_.empty();
}

bool FLevelCatalog::ScanProjectPacks(const std::string& projectsRoot) {
    entries_.clear();
    directory_ = projectsRoot;

    std::error_code ec;
    const std::filesystem::path root(projectsRoot);
    if (!std::filesystem::is_directory(root, ec) || ec) {
        std::cerr << "LevelCatalog: projects root is not a directory: " << projectsRoot << '\n';
        return false;
    }

    for (const auto& packEntry : std::filesystem::directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (!packEntry.is_directory()) {
            continue;
        }
        const std::string packName = packEntry.path().filename().string();
        // Skip shared host / cmake helpers that are not game projects.
        if (packName == "_host" || packName.starts_with('.')) {
            continue;
        }
        const std::filesystem::path levelsDir =
            FPaths::ProjectContentDir(packEntry.path()) / "Levels";

        if (directoryHasLeonLevels(levelsDir)) {
            appendLevelsFromDirectory(entries_, levelsDir, packName);
        }
    }

    std::sort(entries_.begin(), entries_.end(),
              [](const FLevelEntry& a, const FLevelEntry& b) { return a.path < b.path; });

    std::cout << "LevelCatalog: " << entries_.size() << " Level(s) under " << projectsRoot << '\n';
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const FLevelEntry& e = entries_[i];
        std::cout << "  [" << i << "] " << e.pack << "/" << e.name << " (" << e.path << ")\n";
    }
    return !entries_.empty();
}

std::size_t FLevelCatalog::FindIndexByGameModeOrPack(const std::string& id) const {
    if (id.empty()) {
        return entries_.size();
    }
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].gameMode == id || entries_[i].pack == id) {
            return i;
        }
    }
    return entries_.size();
}

std::size_t FLevelCatalog::FindIndexByLevelKey(std::string_view key) const {
    if (key.empty()) {
        return entries_.size();
    }
    const std::string needle = FCString::ToLower(key);
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const FLevelEntry& e = entries_[i];
        if (FCString::ToLower(e.name) == needle) {
            return i;
        }
        const std::string stem = FCString::ToLower(std::filesystem::path(e.path).stem().string());
        if (stem == needle) {
            return i;
        }
        if (FCString::ToLower(e.path) == needle) {
            return i;
        }
    }
    return entries_.size();
}

