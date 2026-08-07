#pragma once

#include <leon/editor/EditorContext.h>
#include <string>

namespace leon::editor {

/// Modal Import Asset dialog (OBJ static / FBX character cook / FBX anim cook).
class ImportDialog {
public:
    void Open();
    void Draw(EditorContext& ctx);

    [[nodiscard]] bool IsOpen() const { return open_; }

private:
    bool open_ = false;
    int mode_ = 0; // EAssetImportMode
    char sourcePath_[512]{};
    char secondaryPath_[512]{};
    char skeletonPath_[512]{};
    char assetName_[128]{};
    bool animLooping_ = true;
    std::string status_;
};

} // namespace leon::editor
