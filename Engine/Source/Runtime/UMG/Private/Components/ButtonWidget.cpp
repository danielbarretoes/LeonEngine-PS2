#include "Components/ButtonWidget.h"

#include <algorithm>
#include "Debug/DebugOverlay.h"

namespace leon {

void ButtonWidget::MeasureDesiredSize(float& outW, float& outH) const {
    float textW = 0.0f;
    float textH = 0.0f;
    DebugOverlay::MeasureText(label_.empty() ? " " : label_, kHudFontScale, textW, textH);
    constexpr float kPadX = 24.0f;
    constexpr float kPadY = 10.0f;
    outW = textW + kPadX * 2.0f;
    outH = std::max(textH, kHudLineHeight) + kPadY * 2.0f;
}

bool ButtonWidget::Contains(float fbX, float fbY) const {
    return fbX >= x_ && fbX <= x_ + w_ && fbY >= y_ && fbY <= y_ + h_;
}

void ButtonWidget::NativePaint(WidgetPaintContext& ctx) {
    if (!IsVisible()) {
        return;
    }
    glm::vec3 bg = backgroundColor_;
    if (!enabled_) {
        bg = backgroundColor_ * 0.55f;
    } else if (selected_) {
        bg = selectedBackgroundColor_;
    } else if (hovered_) {
        bg = hoverBackgroundColor_;
    }
    ctx.DrawRect(x_, y_, w_, h_, bg);

    // Thin top highlight for selected / hover (Unreal button chrome lite).
    if (enabled_ && (selected_ || hovered_)) {
        const glm::vec3 edge = selected_ ? glm::vec3{1.0f, 0.82f, 0.35f}
                                         : glm::vec3{0.55f, 0.50f, 0.35f};
        ctx.DrawRect(x_, y_, w_, 2.0f, edge);
    }

    float textW = 0.0f;
    float textH = 0.0f;
    ctx.MeasureText(label_, kHudFontScale, textW, textH);
    const float textX = x_ + w_ * 0.5f;
    const float textY = y_ + (h_ - textH) * 0.5f;
    const glm::vec3 color = enabled_ ? textColor_ : disabledTextColor_;
    ctx.DrawText(label_, textX, textY, color, kHudFontScale, ETextJustify::Center);
}

} // namespace leon
