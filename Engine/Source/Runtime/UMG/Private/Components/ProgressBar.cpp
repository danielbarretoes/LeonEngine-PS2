#include "Components/ProgressBar.h"

#include <cstdio>
#include <string>


void UProgressBar::NativePaint(FPaintContext& ctx) {
    if (!IsVisible()) {
        return;
    }

    float x = x_;
    float y = y_;
    float w = w_;
    float h = h_;
    if (anchoredBottomCenter_) {
        w = std::min(w_, static_cast<float>(ctx.Width()) * 0.55f);
        h = h_;
        x = (static_cast<float>(ctx.Width()) - w) * 0.5f;
        y = static_cast<float>(ctx.Height()) - h - 36.0f;
    }

    ctx.DrawRect(x, y, w, h, backgroundColor_);
    const float fillW = w * percent_;
    if (fillW > 0.5f) {
        ctx.DrawRect(x, y, fillW, h, fillColor_);
    }
    // 1px border via edge strips.
    ctx.DrawRect(x, y, w, 1.0f, borderColor_);
    ctx.DrawRect(x, y + h - 1.0f, w, 1.0f, borderColor_);
    ctx.DrawRect(x, y, 1.0f, h, borderColor_);
    ctx.DrawRect(x + w - 1.0f, y, 1.0f, h, borderColor_);

    if (showPercentText_) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(percent_ * 100.0f + 0.5f));
        float tw = 0.0f;
        float th = 0.0f;
        ctx.MeasureText(buf, kHudFontScale, tw, th);
        ctx.DrawText(buf, x + w * 0.5f, y + (h - th) * 0.5f, textColor_, kHudFontScale,
                     ETextJustify::Center);
    }
}

