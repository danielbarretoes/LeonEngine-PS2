#include "Components/Image.h"


void UImage::NativePaint(FPaintContext& ctx) {
    if (!IsVisible()) {
        return;
    }

    float x = x_;
    float y = y_;
    float w = w_;
    float h = h_;
    if (fillScreen_) {
        x = 0.0f;
        y = 0.0f;
        w = static_cast<float>(ctx.Width());
        h = static_cast<float>(ctx.Height());
    }

    ctx.DrawRect(x, y, w, h, color_);
    if (drawBorder_ && !fillScreen_) {
        ctx.DrawRect(x, y, w, 2.0f, borderColor_);
        ctx.DrawRect(x, y + h - 2.0f, w, 2.0f, borderColor_);
        ctx.DrawRect(x, y, 2.0f, h, borderColor_);
        ctx.DrawRect(x + w - 2.0f, y, 2.0f, h, borderColor_);
    }
}

