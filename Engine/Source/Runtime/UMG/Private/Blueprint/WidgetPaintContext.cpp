#include "Debug/DebugOverlay.h"
#include "Blueprint/PaintContext.h"


void FPaintContext::DrawLine(float x0, float y0, float x1, float y1, const glm::vec3& color,
                                  float thickness) {
    overlay_.AddScreenLine(x0, y0, x1, y1, color, thickness);
}

void FPaintContext::DrawRect(float x, float y, float w, float h, const glm::vec3& color) {
    overlay_.AddScreenRect(x, y, w, h, color);
}

void FPaintContext::DrawText(const std::string& text, float x, float y,
                                  const glm::vec3& color, float scale, ETextJustify justify) {
    overlay_.AddScreenText(text, x, y, color, scale, justify);
}

void FPaintContext::MeasureText(const std::string& text, float scale, float& outWidth,
                                     float& outHeight) const {
    FDebugOverlay::MeasureText(text, scale, outWidth, outHeight);
}

