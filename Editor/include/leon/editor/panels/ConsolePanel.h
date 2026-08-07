#pragma once

#include <leon/editor/EditorContext.h>
#include <string>

namespace leon::editor {

/// Unreal-like Console: shared Output Log feed + command line.
class ConsolePanel {
public:
    void Draw(EditorContext& ctx);

private:
    bool autoScroll_ = true;
    char input_[256]{};
    void Execute(EditorContext& ctx, const std::string& line);
};

} // namespace leon::editor
