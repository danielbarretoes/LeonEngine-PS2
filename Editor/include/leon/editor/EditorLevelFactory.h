#pragma once

#include <leon/Engine.h>
#include <string>

namespace leon::editor {

enum class ENewLevelTemplate : int {
    Blank = 0,
    Starter = 1,
};

/// Load editor New Level templates from `Engine/Assets/LevelTemplates/*.llev`
/// (not project Templates/). Leaves the level unsaved — caller clears path / marks dirty.
[[nodiscard]] bool CreateLevelFromTemplate(ENewLevelTemplate tmpl, leon::Engine& engine,
                                           std::string& outError);

} // namespace leon::editor
