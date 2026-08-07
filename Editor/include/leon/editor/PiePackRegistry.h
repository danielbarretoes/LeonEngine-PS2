#pragma once

#include <functional>
#include <leon/Engine.h>
#include <leon/gameplay/GameplayRouter.h>
#include <string_view>

namespace leon::editor {

using PieRegisterModesFn = std::function<void(Engine&, GameplayRouter&)>;

/// First-party pack gameplay linked into the Editor for in-process PIE.
struct PiePackInfo {
    /// False → unknown project; Editor falls back to PieGameMode preview.
    bool known = false;
    /// May be empty for packs with only DefaultGameMode (e.g. Smoke).
    PieRegisterModesFn registerModes;
};

/// Lookup by project folder name (`Blank`, `Ps2Lab`, template id, …).
[[nodiscard]] PiePackInfo FindPiePack(std::string_view projectName);

} // namespace leon::editor
