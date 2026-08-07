#pragma once

#include <leon/editor/EditorContext.h>

namespace leon::editor {

class OutputLogPanel {
public:
    void Draw(EditorContext& ctx);

private:
    bool autoScroll_ = true;
    int filterLevel_ = 0; // 0=all, 1=warn+, 2=error
};

} // namespace leon::editor
