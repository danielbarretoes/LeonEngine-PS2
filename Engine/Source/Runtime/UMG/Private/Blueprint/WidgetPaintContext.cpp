#include "Debug/DebugOverlay.h"
#include "Blueprint/WidgetPaintContext.h"


void WidgetPaintContext::DrawLine(float x0, float y0, float x1, float y1, const glm::vec3& color,
                                  float thickness) {
    overlay_.AddScreenLine(x0, y0, x1, y1, color, thickness);
}

void WidgetPaintContext::DrawRect(float x, float y, float w, float h, const glm::vec3& color) {
    overlay_.AddScreenRect(x, y, w, h, color);
}

void WidgetPaintContext::DrawText(const std::string& text, float x, float y,
                                  const glm::vec3& color, float scale, ETextJustify justify) {
    overlay_.AddScreenText(text, x, y, color, scale, justify);
}

void WidgetPaintContext::MeasureText(const std::string& text, float scale, float& outWidth,
                                     float& outHeight) const {
    DebugOverlay::MeasureText(text, scale, outWidth, outHeight);
}

