#include "Components/MenuListWidget.h"
#include "EKey.h"

#include <algorithm>
#include <cmath>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "Window.h"
#include "EKey.h"

namespace leon {
namespace {

/// Lines before the first menu item in BuildPaintText (title + blank separator).
[[nodiscard]] int ItemStartLine(const std::string& title) {
    if (title.empty()) {
        return 1; // leading "\n\n" still inserts one blank row before items
    }
    int titleLines = 1;
    for (char c : title) {
        if (c == '\n') {
            ++titleLines;
        }
    }
    return titleLines + 1;
}

} // namespace

void MenuListWidget::SetItems(std::vector<Item> items) {
    items_ = std::move(items);
    if (items_.empty()) {
        selected_ = 0;
        return;
    }
    selected_ = std::clamp(selected_, 0, static_cast<int>(items_.size()) - 1);
    // Prefer a selectable row (non-empty id) — status labels are not activatable.
    if (items_[static_cast<std::size_t>(selected_)].id.empty()) {
        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            if (!items_[static_cast<std::size_t>(i)].id.empty()) {
                selected_ = i;
                break;
            }
        }
    }
}

void MenuListWidget::SetSelectedIndex(int index) {
    if (items_.empty()) {
        selected_ = 0;
        return;
    }
    selected_ = std::clamp(index, 0, static_cast<int>(items_.size()) - 1);
}

void MenuListWidget::ResetEdges() {
    upWasDown_ = downWasDown_ = enterWasDown_ = mouseWasDown_ = true;
    // Keyboard only — mouse must stay usable on the first click after travel.
    ignoreActivateSeconds_ = 0.35f;
}

int MenuListWidget::CountLines(const std::string& text) const {
    if (text.empty()) {
        return 0;
    }
    int n = 1;
    for (char c : text) {
        if (c == '\n') {
            ++n;
        }
    }
    return n;
}

std::string MenuListWidget::BuildPaintText() const {
    std::string text = title_;
    text += "\n\n";
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        text += (i == selected_) ? "> " : "  ";
        text += items_[static_cast<std::size_t>(i)].label;
        text += '\n';
    }
    if (!hint_.empty()) {
        text += "\n";
        text += hint_;
    }
    return text;
}

void MenuListWidget::CacheLayout(int /*viewportW*/, int viewportH) {
    viewportH_ = viewportH;
    lineH_ = kHudLineHeight;
    if (items_.empty() || viewportH <= 0) {
        itemsTopPx_ = 0.0f;
        return;
    }
    // Same vertical center as NativePaint / DebugOverlay::MeasureText (14 * scale per line).
    const int lines = CountLines(BuildPaintText());
    const float h = lineH_ * static_cast<float>(std::max(lines, 1));
    float top = (static_cast<float>(viewportH) - h) * 0.5f;
    top = std::clamp(top, 10.0f, std::max(10.0f, static_cast<float>(viewportH) - h - 10.0f));
    itemsTopPx_ = top + lineH_ * static_cast<float>(ItemStartLine(title_));
}

void MenuListWidget::NativePaint(WidgetPaintContext& ctx) {
    if (items_.empty()) {
        return;
    }
    CacheLayout(ctx.Width(), ctx.Height());
    const std::string text = BuildPaintText();
    float w = 0.0f;
    float h = 0.0f;
    ctx.MeasureText(text, kHudFontScale, w, h);
    float top = (static_cast<float>(ctx.Height()) - h) * 0.5f;
    top = std::clamp(top, 10.0f, std::max(10.0f, static_cast<float>(ctx.Height()) - h - 10.0f));
    ctx.DrawText(text, static_cast<float>(ctx.Width()) * 0.5f, top, color_, kHudFontScale,
                 ETextJustify::Center);
}

std::string MenuListWidget::TickInput(Window& window, bool cursorCaptured, float deltaTime) {
    if (items_.empty()) {
        return {};
    }
    if (ignoreActivateSeconds_ > 0.0f) {
        ignoreActivateSeconds_ = std::max(0.0f, ignoreActivateSeconds_ - deltaTime);
    }

    const bool up = window.IsKeyPressed(EKey::Up) || window.IsKeyPressed(EKey::W);
    const bool down = window.IsKeyPressed(EKey::Down) || window.IsKeyPressed(EKey::S);
    const bool enter = window.IsKeyPressed(EKey::Enter) ||
                       window.IsKeyPressed(EKey::KpEnter) ||
                       window.IsKeyPressed(EKey::Space);
    const bool mouse = window.IsMouseButtonDown(EMouseButton::Left);

    auto stepSelectable = [this](int delta) {
        const int n = static_cast<int>(items_.size());
        if (n <= 0) {
            return;
        }
        int idx = selected_;
        for (int guard = 0; guard < n; ++guard) {
            idx = (idx + delta + n) % n;
            if (!items_[static_cast<std::size_t>(idx)].id.empty()) {
                selected_ = idx;
                return;
            }
        }
    };

    if (up && !upWasDown_) {
        stepSelectable(-1);
    }
    if (down && !downWasDown_) {
        stepSelectable(1);
    }

    const bool allowKeyboardActivate = ignoreActivateSeconds_ <= 0.0f;
    std::string activated;
    if (allowKeyboardActivate && enter && !enterWasDown_) {
        const std::string& id = items_[static_cast<std::size_t>(selected_)].id;
        if (!id.empty()) {
            activated = id;
        }
    }

    // Mouse activate is never gated by travel lockout (only edges / capture).
    if (mouse && !mouseWasDown_ && !cursorCaptured) {
        double mx = 0.0;
        double my = 0.0;
        window.GetCursorPos(mx, my);
        int winW = 0;
        int winH = 0;
        window.GetWindowSize(winW, winH);
        int fbW = 0;
        int fbH = 0;
        window.GetFramebufferSize(fbW, fbH);
        winW = std::max(winW, 1);
        winH = std::max(winH, 1);
        if (fbW > 0 && fbH > 0) {
            CacheLayout(fbW, fbH);
            const float fbY =
                static_cast<float>(my) * static_cast<float>(fbH) / static_cast<float>(winH);
            // Inclusive band with small pad so the first row is not a dead zone.
            const float rel = fbY - itemsTopPx_ + (lineH_ * 0.15f);
            const int hit = static_cast<int>(std::floor(rel / lineH_));
            if (hit >= 0 && hit < static_cast<int>(items_.size())) {
                selected_ = hit;
                const std::string& id = items_[static_cast<std::size_t>(selected_)].id;
                if (!id.empty()) {
                    activated = id;
                }
            }
        }
    }

    upWasDown_ = up;
    downWasDown_ = down;
    enterWasDown_ = enter;
    mouseWasDown_ = mouse;
    return activated;
}

} // namespace leon
