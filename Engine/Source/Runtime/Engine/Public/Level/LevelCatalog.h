#pragma once

#include <string>
#include <string_view>
#include <vector>


struct LevelEntry {
    std::string name;     // from the level document name, else filename stem
    std::string path;     // resolved .llev path
    std::string pack;     // project folder name under `Projects/`
    std::string gameMode; // Unreal-like GameMode Override
};

/// Discovers binary Leon Level files (`.llev`) under project packs or a flat directory.
class LevelCatalog {
public:
    /// Scan a flat directory of `*.llev` (unit tests / tools).
    bool Scan(const std::string& directory);

    /// Scan every pack under `projectsRoot/` for `Content/Levels/*.llev`.
    bool ScanProjectPacks(const std::string& projectsRoot);

    /// Scan one pack (`Projects/<pack>/`) for `Content/Levels/*.llev`
    /// (also `<pack>/Levels` via ProjectContentDirectory pre-Content layout).
    bool ScanPack(const std::string& packDirectory);

    /// First catalog index whose `gameMode` or `pack` equals `id`, or `NumEntries()` if none.
    [[nodiscard]] std::size_t FindIndexByGameModeOrPack(const std::string& id) const;

    /// Match `LevelEntry.name`, path stem, or full path (case-insensitive). Returns `NumEntries()` if none.
    [[nodiscard]] std::size_t FindIndexByLevelKey(std::string_view key) const;

    [[nodiscard]] const std::vector<LevelEntry>& Entries() const { return entries_; }
    [[nodiscard]] bool IsEmpty() const { return entries_.empty(); }
    [[nodiscard]] std::size_t NumEntries() const { return entries_.size(); }
    [[nodiscard]] const std::string& Directory() const { return directory_; }

private:
    std::string directory_;
    std::vector<LevelEntry> entries_;
};

