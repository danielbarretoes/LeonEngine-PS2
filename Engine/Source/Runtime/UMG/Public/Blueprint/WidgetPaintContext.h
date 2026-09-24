#pragma once

#include <glm/vec3.hpp>
#include "Fonts/TextLayout.h"
#include <string>

namespace leon {

class DebugOverlay;

/// Immediate screen-space draw for UserWidget::NativePaint (pixel coords, top-left origin).
/// Unreal analogy: FPaintContext / Slate draw elements (lite).
class WidgetPaintContext {
public:
    WidgetPaintContext(DebugOverlay& overlay, int framebufferWidth, int framebufferHeight)
        : overlay_(overlay), width_(framebufferWidth), height_(framebufferHeight) {}

    [[nodiscard]] int Width() const { return width_; }
    [[nodiscard]] int Height() const { return height_; }

    void DrawLine(float x0, float y0, float x1, float y1, const glm::vec3& color,
                  float thickness = 2.0f);
    void DrawRect(float x, float y, float w, float h, const glm::vec3& color);

    /// Draw multiline text. `x` is left/center/right of each line per `justify`.
    void DrawText(const std::string& text, float x, float y, const glm::vec3& color,
                  float scale = kHudFontScale, ETextJustify justify = ETextJustify::Left);

    void MeasureText(const std::string& text, float scale, float& outWidth, float& outHeight) const;

private:
    DebugOverlay& overlay_;
    int width_ = 0;
    int height_ = 0;
};

} // namespace leon
