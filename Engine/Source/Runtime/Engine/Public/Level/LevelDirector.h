#pragma once

#include "Debug/DebugOverlay.h"
#include "Level/LevelAnimation.h"
#include "Level/LevelCatalog.h"
#include <string>
#include <string_view>

namespace leon {

class Engine;

/// Scans game levels, loads them into an Engine, and draws a bottom-right browser
/// (`< name (i/n) >`). Switch with `[` / `]`, digit keys `1`–`9`, or mouse on the arrows.
class LevelDirector {
public:
    bool Initialize(const std::string& shaderDirectory);
    void Shutdown();
    [[nodiscard]] EShaderReloadResult ReloadShaders(bool force = false);

    /// Discover `.llev` levels under `projectsRoot/<pack>/Content/Levels/` and load the first.
    bool ScanAndLoad(Engine& engine, const std::string& projectsDirectory);

    /// Discover levels for a single project folder (`Projects/<pack>/`) and load
    /// `preferredLevelKey` when set (LevelEntry.name / stem); otherwise the first entry.
    bool ScanPackAndLoad(Engine& engine, const std::string& packDirectory,
                         std::string_view preferredLevelKey = {});

    /// Load by catalog index. On failure the previous Level contents may be cleared;
    /// CurrentIndex is only updated after a successful load.
    bool LoadIndex(Engine& engine, std::size_t index);
    /// Load by LevelEntry.name / path stem (net travel key). Returns false if unknown.
    bool LoadByKey(Engine& engine, std::string_view levelKey);
    bool Next(Engine& engine);
    bool Previous(Engine& engine);

    /// Apply spin / bob / point-light orbit from the loaded Level.
    void Update(Engine& engine, float deltaTime);

    /// Draw the bottom-right chrome after the 3D + stats pass.
    void DrawUi(int framebufferWidth, int framebufferHeight);

    /// Handle `[` `]`, digits `1`–`9`, and clicks on `<` `>`.
    /// Returns true while camera drag should be blocked.
    bool HandleUiInput(Engine& engine);

    [[nodiscard]] bool IsEmpty() const { return catalog_.IsEmpty(); }
    [[nodiscard]] std::size_t CurrentIndex() const { return currentIndex_; }
    [[nodiscard]] const LevelCatalog& Catalog() const { return catalog_; }

    /// When false, `[`/`]` chrome and input are disabled (menus / shipping UI).
    void SetBrowserVisible(bool visible) { browserVisible_ = visible; }
    [[nodiscard]] bool IsBrowserVisible() const { return browserVisible_; }

private:
    void refreshChrome(int framebufferWidth, int framebufferHeight);
    void layoutChrome(int framebufferWidth, int framebufferHeight);
    [[nodiscard]] bool hitPrev(float x, float y) const;
    [[nodiscard]] bool hitNext(float x, float y) const;
    void cursorFramebuffer(Engine& engine, float& outX, float& outY) const;

    LevelCatalog catalog_;
    LevelAnimation animation_;
    DebugOverlay chrome_;

    std::size_t currentIndex_ = 0;
    float elapsed_ = 0.0f;

    float prevMinX_ = 0.0f;
    float prevMaxX_ = 0.0f;
    float nextMinX_ = 0.0f;
    float nextMaxX_ = 0.0f;
    float chromeMinY_ = 0.0f;
    float chromeMaxY_ = 0.0f;
    int layoutFbWidth_ = 0;
    int layoutFbHeight_ = 0;

    bool mouseWasDown_ = false;
    bool ignoreDrag_ = false;
    bool keyPrevDown_ = false;
    bool keyNextDown_ = false;
    bool digitWasDown_[9] = {};
    bool browserVisible_ = true;
};

} // namespace leon
