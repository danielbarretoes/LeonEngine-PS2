#pragma once

#include "Components/ButtonWidget.h"
#include "Blueprint/UserWidget.h"
#include <memory>
#include <string>
#include <vector>

namespace leon {

class Window;

/// Unreal-like UVerticalBox (lite): title + stacked ButtonWidgets + hint.
/// Add via HUD::AddWidget; call TickInput each frame from GameMode (same as MenuListWidget).
class VerticalBoxWidget : public UserWidget {
public:
    void SetTitle(std::string title) { title_ = std::move(title); }
    void SetHint(std::string hint) { hint_ = std::move(hint); }

    void ClearChildren();

    /// Append a UButton-like child. Empty `id` = non-activatable status row.
    ButtonWidget* AddButton(std::string id, std::string label);

    [[nodiscard]] int NumButtons() const { return static_cast<int>(buttons_.size()); }
    [[nodiscard]] ButtonWidget* GetButton(int index);
    [[nodiscard]] const ButtonWidget* GetButton(int index) const;

    [[nodiscard]] int SelectedIndex() const { return selected_; }
    void SetSelectedIndex(int index);

    /// Seed edges as pressed + short keyboard activate lockout (safe after travel).
    void ResetEdges();

    /// Returns activated button id this frame (empty if none).
    [[nodiscard]] std::string TickInput(Window& window, bool cursorCaptured, float deltaTime);

    void NativePaint(WidgetPaintContext& ctx) override;

private:
    void CacheLayout(int viewportW, int viewportH);
    void SnapSelectionToSelectable();
    void StepSelectable(int delta);
    void ApplySelectionVisuals();

    std::string title_ = "Menu";
    std::string hint_;
    std::vector<std::unique_ptr<ButtonWidget>> buttons_;
    int selected_ = 0;

    bool upWasDown_ = false;
    bool downWasDown_ = false;
    bool enterWasDown_ = false;
    bool mouseWasDown_ = false;
    /// Suppresses keyboard activate only (ghost Enter after travel); mouse stays live.
    float ignoreActivateSeconds_ = 0.0f;

    float boxX_ = 0.0f;
    float boxY_ = 0.0f;
    float boxW_ = 0.0f;
    float titleH_ = 0.0f;
    float hintH_ = 0.0f;
    float buttonGap_ = 8.0f;
    float minButtonW_ = 220.0f;
};

} // namespace leon
