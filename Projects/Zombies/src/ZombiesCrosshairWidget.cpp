#include "ZombiesCrosshairWidget.h"

#include <glm/vec3.hpp>

#include <algorithm>

namespace game {

void ZombiesCrosshairWidget::NativePaint(leon::WidgetPaintContext& ctx) {
    const float cx = static_cast<float>(ctx.Width()) * 0.5f;
    const float cy = static_cast<float>(ctx.Height()) * 0.5f;

    constexpr float kGap = 5.0f;
    constexpr float kArm = 12.0f;
    constexpr float kThick = 2.0f;
    const float pulse = std::clamp(Pulse, 0.35f, 1.0f);
    const glm::vec3 color{0.95f * pulse, 0.95f * pulse, 0.85f * pulse};
    const glm::vec3 outline{0.05f, 0.05f, 0.05f};

    // Soft outline (1px thicker) then bright arms — readable on light/dark scenes.
    auto arm = [&](float x0, float y0, float x1, float y1) {
        ctx.DrawLine(x0, y0, x1, y1, outline, kThick + 2.0f);
        ctx.DrawLine(x0, y0, x1, y1, color, kThick);
    };

    arm(cx - kGap - kArm, cy, cx - kGap, cy);
    arm(cx + kGap, cy, cx + kGap + kArm, cy);
    arm(cx, cy - kGap - kArm, cx, cy - kGap);
    arm(cx, cy + kGap, cx, cy + kGap + kArm);

    // Center micro-dot.
    ctx.DrawRect(cx - 1.0f, cy - 1.0f, 2.0f, 2.0f, color);
}

} // namespace game
