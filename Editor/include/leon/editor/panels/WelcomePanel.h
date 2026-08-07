#pragma once

#include <leon/editor/EditorProject.h>
#include <string>
#include <vector>

namespace leon::editor {

/// Full-screen project hub shown before the main editor layout.
class WelcomePanel {
public:
    WelcomePanel() = default;
    ~WelcomePanel();

    WelcomePanel(const WelcomePanel&) = delete;
    WelcomePanel& operator=(const WelcomePanel&) = delete;

    void Reset();
    void Draw(EditorProjectService& projects);

    [[nodiscard]] bool HasPendingOpen() const { return !pendingOpenPath_.empty(); }
    [[nodiscard]] const std::string& PendingOpenPath() const { return pendingOpenPath_; }
    void ClearPendingOpen() { pendingOpenPath_.clear(); }

private:
    void DrawCreateModal(EditorProjectService& projects);
    void RequestOpen(const std::string& path);
    void EnsureBrandTexture();
    void DestroyBrandTexture();

    char nameBuf_[64]{};
    char displayBuf_[96]{};
    char parentBuf_[512]{};
    std::string statusMessage_;
    bool statusIsError_ = false;
    bool openCreatePopup_ = false;
    int selectedTemplate_ = 0;
    std::vector<std::string> templateIds_;
    std::string pendingOpenPath_;
    unsigned int brandTexture_ = 0;
    int brandWidth_ = 0;
    int brandHeight_ = 0;
    bool brandLoadAttempted_ = false;
};

} // namespace leon::editor
