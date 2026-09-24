#include "Debug/DebugOverlay.h"
#include "Blueprint/PaintContext.h"


void FPaintContext::DrawLine(float X0, float Y0, float X1, float Y1, const glm::vec3& Color,
                                  float Thickness) {
    Overlay.AddScreenLine(X0, Y0, X1, Y1, Color, Thickness);
}

void FPaintContext::DrawRect(float X, float Y, float W, float H, const glm::vec3& Color) {
    Overlay.AddScreenRect(X, Y, W, H, Color);
}

void FPaintContext::DrawText(const std::string& Text, float X, float Y,
                                  const glm::vec3& Color, float Scale, ETextJustify Justify) {
    Overlay.AddScreenText(Text, X, Y, Color, Scale, Justify);
}

void FPaintContext::MeasureText(const std::string& Text, float Scale, float& OutWidth,
                                     float& OutHeight) const {
    FDebugOverlay::MeasureText(Text, Scale, OutWidth, OutHeight);
}

