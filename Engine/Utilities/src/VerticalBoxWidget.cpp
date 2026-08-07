#include <leon/ui/VerticalBoxWidget.h>

#include <algorithm>
#include <cmath>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <leon/core/Window.h>
#include <leon/debug/DebugOverlay.h>

namespace leon {

void VerticalBoxWidget::ClearChildren() {
    buttons_.clear();
    selected_ = 0;
}

ButtonWidget* VerticalBoxWidget::AddButton(std::string id, std::string label) {
    auto button = std::make_unique<ButtonWidget>();
    button->SetId(std::move(id));
    button->SetLabel(std::move(label));
    ButtonWidget* raw = button.get();
    buttons_.push_back(std::move(button));
    SnapSelectionToSelectable();
    ApplySelectionVisuals();
    return raw;
}

ButtonWidget* VerticalBoxWidget::GetButton(int index) {
    if (index < 0 || index >= static_cast<int>(buttons_.size())) {
        return nullptr;
    }
    return buttons_[static_cast<std::size_t>(index)].get();
}

const ButtonWidget* VerticalBoxWidget::GetButton(int index) const {
    if (index < 0 || index >= static_cast<int>(buttons_.size())) {
        return nullptr;
    }
    return buttons_[static_cast<std::size_t>(index)].get();
}

void VerticalBoxWidget::SetSelectedIndex(int index) {
    if (buttons_.empty()) {
        selected_ = 0;
        ApplySelectionVisuals();
        return;
    }
    selected_ = std::clamp(index, 0, static_cast<int>(buttons_.size()) - 1);
    ApplySelectionVisuals();
}

void VerticalBoxWidget::ResetEdges() {
    upWasDown_ = downWasDown_ = enterWasDown_ = mouseWasDown_ = true;
    // Keyboard only — mouse must stay usable on the first click after travel.
    ignoreActivateSeconds_ = 0.35f;
}

void VerticalBoxWidget::SnapSelectionToSelectable() {
    if (buttons_.empty()) {
        selected_ = 0;
        return;
    }
    selected_ = std::clamp(selected_, 0, static_cast<int>(buttons_.size()) - 1);
    if (!buttons_[static_cast<std::size_t>(selected_)]->GetId().empty() &&
        buttons_[static_cast<std::size_t>(selected_)]->IsEnabled()) {
        return;
    }
    for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) {
        if (!buttons_[static_cast<std::size_t>(i)]->GetId().empty() &&
            buttons_[static_cast<std::size_t>(i)]->IsEnabled()) {
            selected_ = i;
            return;
        }
    }
}

void VerticalBoxWidget::StepSelectable(int delta) {
    const int n = static_cast<int>(buttons_.size());
    if (n <= 0) {
        return;
    }
    int idx = selected_;
    for (int guard = 0; guard < n; ++guard) {
        idx = (idx + delta + n) % n;
        ButtonWidget* button = buttons_[static_cast<std::size_t>(idx)].get();
        if (!button->GetId().empty() && button->IsEnabled()) {
            selected_ = idx;
            ApplySelectionVisuals();
            return;
        }
    }
}

void VerticalBoxWidget::ApplySelectionVisuals() {
    for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) {
        buttons_[static_cast<std::size_t>(i)]->SetSelected(i == selected_);
    }
}

void VerticalBoxWidget::CacheLayout(int viewportW, int viewportH) {
    if (viewportW <= 0 || viewportH <= 0) {
        return;
    }

    float maxButtonW = minButtonW_;
    float buttonsH = 0.0f;
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        float dw = 0.0f;
        float dh = 0.0f;
        buttons_[i]->MeasureDesiredSize(dw, dh);
        maxButtonW = std::max(maxButtonW, dw);
        buttonsH += dh;
        if (i + 1 < buttons_.size()) {
            buttonsH += buttonGap_;
        }
    }

    titleH_ = 0.0f;
    if (!title_.empty()) {
        float tw = 0.0f;
        DebugOverlay::MeasureText(title_, kHudFontScale, tw, titleH_);
        titleH_ += kHudLineHeight; // blank separator under title
    }

    hintH_ = 0.0f;
    if (!hint_.empty()) {
        float hw = 0.0f;
        DebugOverlay::MeasureText(hint_, kHudFontScale, hw, hintH_);
        hintH_ += 12.0f; // gap above hint
    }

    boxW_ = maxButtonW;
    const float totalH = titleH_ + buttonsH + hintH_;
    boxX_ = (static_cast<float>(viewportW) - boxW_) * 0.5f;
    boxY_ = std::clamp((static_cast<float>(viewportH) - totalH) * 0.5f, 10.0f,
                       std::max(10.0f, static_cast<float>(viewportH) - totalH - 10.0f));

    float y = boxY_ + titleH_;
    for (std::unique_ptr<ButtonWidget>& button : buttons_) {
        float dw = 0.0f;
        float dh = 0.0f;
        button->MeasureDesiredSize(dw, dh);
        button->SetSize(boxW_, dh);
        button->SetPosition(boxX_, y);
        y += dh + buttonGap_;
    }
}

void VerticalBoxWidget::NativePaint(WidgetPaintContext& ctx) {
    if (!IsVisible()) {
        return;
    }
    CacheLayout(ctx.Width(), ctx.Height());

    if (!title_.empty()) {
        ctx.DrawText(title_, static_cast<float>(ctx.Width()) * 0.5f, boxY_,
                     glm::vec3{1.0f, 0.82f, 0.35f}, kHudFontScale, ETextJustify::Center);
    }

    for (std::unique_ptr<ButtonWidget>& button : buttons_) {
        button->NativePaint(ctx);
    }

    if (!hint_.empty()) {
        float buttonsBottom = boxY_ + titleH_;
        for (const std::unique_ptr<ButtonWidget>& button : buttons_) {
            buttonsBottom = button->GetY() + button->GetHeight();
        }
        const float hintY = buttonsBottom + 12.0f;
        ctx.DrawText(hint_, static_cast<float>(ctx.Width()) * 0.5f, hintY,
                     glm::vec3{0.65f, 0.65f, 0.60f}, kHudFontScale, ETextJustify::Center);
    }
}

std::string VerticalBoxWidget::TickInput(Window& window, bool cursorCaptured, float deltaTime) {
    if (buttons_.empty()) {
        return {};
    }
    if (ignoreActivateSeconds_ > 0.0f) {
        ignoreActivateSeconds_ = std::max(0.0f, ignoreActivateSeconds_ - deltaTime);
    }

    int fbW = 0;
    int fbH = 0;
    window.GetFramebufferSize(fbW, fbH);
    if (fbW > 0 && fbH > 0) {
        CacheLayout(fbW, fbH);
    }

    const bool up = window.IsKeyPressed(GLFW_KEY_UP) || window.IsKeyPressed(GLFW_KEY_W);
    const bool down = window.IsKeyPressed(GLFW_KEY_DOWN) || window.IsKeyPressed(GLFW_KEY_S);
    const bool enter = window.IsKeyPressed(GLFW_KEY_ENTER) ||
                       window.IsKeyPressed(GLFW_KEY_KP_ENTER) ||
                       window.IsKeyPressed(GLFW_KEY_SPACE);
    const bool mouse = window.IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);

    if (up && !upWasDown_) {
        StepSelectable(-1);
    }
    if (down && !downWasDown_) {
        StepSelectable(1);
    }

    const bool allowKeyboardActivate = ignoreActivateSeconds_ <= 0.0f;
    std::string activated;
    if (allowKeyboardActivate && enter && !enterWasDown_) {
        ButtonWidget* button = GetButton(selected_);
        if (button != nullptr && button->IsEnabled() && !button->GetId().empty()) {
            activated = button->GetId();
        }
    }

    // Hover + click (never gated by travel lockout).
    if (!cursorCaptured && fbW > 0 && fbH > 0) {
        double mx = 0.0;
        double my = 0.0;
        window.GetCursorPos(mx, my);
        int winW = 0;
        int winH = 0;
        window.GetWindowSize(winW, winH);
        winW = std::max(winW, 1);
        winH = std::max(winH, 1);
        const float fbX =
            static_cast<float>(mx) * static_cast<float>(fbW) / static_cast<float>(winW);
        const float fbY =
            static_cast<float>(my) * static_cast<float>(fbH) / static_cast<float>(winH);

        for (std::unique_ptr<ButtonWidget>& button : buttons_) {
            button->SetHovered(button->Contains(fbX, fbY));
        }

        if (mouse && !mouseWasDown_) {
            for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) {
                ButtonWidget* button = buttons_[static_cast<std::size_t>(i)].get();
                if (!button->Contains(fbX, fbY)) {
                    continue;
                }
                selected_ = i;
                ApplySelectionVisuals();
                if (button->IsEnabled() && !button->GetId().empty()) {
                    activated = button->GetId();
                }
                break;
            }
        }
    } else {
        for (std::unique_ptr<ButtonWidget>& button : buttons_) {
            button->SetHovered(false);
        }
    }

    upWasDown_ = up;
    downWasDown_ = down;
    enterWasDown_ = enter;
    mouseWasDown_ = mouse;
    return activated;
}

} // namespace leon
