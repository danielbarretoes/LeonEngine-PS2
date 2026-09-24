#pragma once

#include <string>


/// Discover/load a project pack under `Projects/<name>/`.
struct ProjectPack {
    std::string name;
    std::string rootDirectory;
    /// From `leon.game.json` `defaultLevel` (e.g. `Levels/MainMenu.llev`), may be empty.
    std::string defaultLevel;

    /// Resolve pack root (`Projects/<name>` via asset path search) and read leon.game.json.
    [[nodiscard]] static ProjectPack Resolve(const char* packName);

    /// Catalog travel key for `defaultLevel` (path stem), or empty.
    [[nodiscard]] std::string DefaultLevelKey() const;
};

