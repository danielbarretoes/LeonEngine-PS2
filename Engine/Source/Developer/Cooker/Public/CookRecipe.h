#pragma once

#include <string>

namespace leon::tools {

/// Run a leon-cook recipe JSON (`steps` array: character | anim | staticmesh).
/// Relative paths resolve next to the recipe file. Returns process-style exit code (0 ok).
[[nodiscard]] int RunCookRecipeFile(const std::string& recipePath);

} // namespace leon::tools
