#pragma once

#include <glm/vec3.hpp>
#include <leon/ui/TextLayout.h>
#include <leon/ui/UserWidget.h>
#include <string>
#include <vector>

namespace leon {

class Window;

/// Unreal-like vertical text menu (UMG ListView lite): arrows / Enter / click.
/// Add via HUD::AddWidget; call TickInput each frame from GameMode.
class MenuListWidget : public UserWidget {
public:
    struct Item {
        std::string id;
        std::string label;
    };

    void SetTitle(std::string title) { title_ = std::move(title); }
    void SetHint(std::string hint) { hint_ = std::move(hint); }
    void SetItems(std::vector<Item> items);
    void SetColor(const glm::vec3& color) { color_ = color; }

    [[nodiscard]] int SelectedIndex() const { return selected_; }
    void SetSelectedIndex(int index);

    /// Seed edges as pressed + short activate lockout (safe after travel).
    void ResetEdges();

    /// Returns activated item id this frame (empty if none).
    [[nodiscard]] std::string TickInput(Window& window, bool cursorCaptured, float deltaTime);

    void NativePaint(WidgetPaintContext& ctx) override;

private:
    [[nodiscard]] std::string BuildPaintText() const;
    [[nodiscard]] int CountLines(const std::string& text) const;
    void CacheLayout(int viewportW, int viewportH);

    std::string title_ = "Menu";
    std::string hint_;
    std::vector<Item> items_;
    int selected_ = 0;
    glm::vec3 color_{1.0f, 0.82f, 0.35f};

    bool upWasDown_ = false;
    bool downWasDown_ = false;
    bool enterWasDown_ = false;
    bool mouseWasDown_ = false;
    /// Suppresses keyboard activate only (ghost Enter after travel); mouse stays live.
    float ignoreActivateSeconds_ = 0.0f;

    // Layout cached for hit-testing (Paint + TickInput).
    float itemsTopPx_ = 0.0f;
    float lineH_ = kHudLineHeight;
    int viewportH_ = 0;
};

} // namespace leon
