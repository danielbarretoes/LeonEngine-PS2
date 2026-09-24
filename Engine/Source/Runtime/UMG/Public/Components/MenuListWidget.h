#pragma once

#include <glm/vec3.hpp>
#include "Fonts/TextLayout.h"
#include "Blueprint/UserWidget.h"
#include <string>
#include <vector>

class FGenericWindow;

/// Unreal-like vertical text menu (UMG ListView lite): arrows / Enter / click.
/// Add via HUD::AddWidget; call TickInput each frame from GameMode.
class UMenuListWidget : public UUserWidget {
public:
    struct FItem {
        std::string id;
        std::string label;
    };

    void SetTitle(std::string title) { title_ = std::move(title); }
    void SetHint(std::string hint) { hint_ = std::move(hint); }
    void SetItems(std::vector<FItem> items);
    void SetColor(const glm::vec3& color) { color_ = color; }

    [[nodiscard]] int SelectedIndex() const { return selected_; }
    void SetSelectedIndex(int index);

    /// Seed edges as pressed + short activate lockout (safe after travel).
    void ResetEdges();

    /// Returns activated item id this frame (empty if none).
    [[nodiscard]] std::string TickInput(FGenericWindow& window, bool cursorCaptured, float deltaTime);

    void NativePaint(FPaintContext& ctx) override;

private:
    [[nodiscard]] std::string BuildPaintText() const;
    [[nodiscard]] int CountLines(const std::string& text) const;
    void CacheLayout(int viewportW, int viewportH);

    std::string title_ = "Menu";
    std::string hint_;
    std::vector<FItem> items_;
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

