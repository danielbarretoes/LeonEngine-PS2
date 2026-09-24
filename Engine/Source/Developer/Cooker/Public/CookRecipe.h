#pragma once

#include <string>


/// Run a leon-cook recipe JSON (`steps` array: character | anim | staticmesh).
/// Relative paths resolve next to the recipe file. Returns process-style exit code (0 ok).
[[nodiscard]] int RunCookRecipeFile(const std::string& recipePath);

