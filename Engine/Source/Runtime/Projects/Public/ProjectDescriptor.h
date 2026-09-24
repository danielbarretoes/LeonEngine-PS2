#pragma once

#include "CoreTypes.h"

#include <string>


/// Discover/load a project pack under `Projects/<name>/`.
struct PROJECTS_API FProjectDescriptor {
    std::string Name;
    std::string RootDirectory;
    /// From `leon.game.json` `defaultLevel` (e.g. `Levels/MainMenu.llev`), may be empty.
    std::string DefaultLevel;

    /// Resolve pack root (`Projects/<name>` via asset path search) and read leon.game.json.
    [[nodiscard]] static FProjectDescriptor Resolve(const char* PackName);

    /// Catalog travel key for `defaultLevel` (path stem), or empty.
    [[nodiscard]] std::string DefaultLevelKey() const;
};

