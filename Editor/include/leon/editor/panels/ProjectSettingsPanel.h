#pragma once

#include <leon/editor/EditorContext.h>
#include <string>

namespace leon::editor {

/// Pack-wide settings from `leon.game.json` (Unreal-like Project Settings).
/// Distinct from World Settings, which are per-level.
class ProjectSettingsPanel {
public:
    void Draw(EditorContext& ctx);

private:
    std::string descriptionDraft_;
    std::string descriptionProjectKey_;
};

} // namespace leon::editor
