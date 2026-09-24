#include <algorithm>
#include "Components/TextBlockWidget.h"


void TextBlockWidget::NativePaint(WidgetPaintContext& ctx) {
    if (text_.empty()) {
        return;
    }
    float x = x_;
    float y = y_;
    if (centeredOnScreen_) {
        float w = 0.0f;
        float h = 0.0f;
        ctx.MeasureText(text_, scale_, w, h);
        x = static_cast<float>(ctx.Width()) * 0.5f;
        y = std::clamp((static_cast<float>(ctx.Height()) - h) * 0.5f, 10.0f,
                       std::max(10.0f, static_cast<float>(ctx.Height()) - h - 10.0f));
        justify_ = ETextJustify::Center;
    }
    ctx.DrawText(text_, x, y, color_, scale_, justify_);
}

