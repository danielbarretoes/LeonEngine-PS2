#pragma once

#include <glm/vec3.hpp>
#include "Fonts/TextLayout.h"
#include <string>


class FDebugOverlay;

/// Immediate screen-space draw for UUserWidget::NativePaint (pixel coords, top-left origin).
/// Unreal analogy: FPaintContext / Slate draw elements (lite).
class FPaintContext {
public:
    FPaintContext(FDebugOverlay& InOverlay, int FramebufferWidth, int FramebufferHeight)
        : Overlay(InOverlay), Width(FramebufferWidth), Height(FramebufferHeight) {}

    [[nodiscard]] int GetWidth() const { return Width; }
    [[nodiscard]] int GetHeight() const { return Height; }

    void DrawLine(float X0, float Y0, float X1, float Y1, const glm::vec3& Color,
                  float Thickness = 2.0f);
    void DrawRect(float X, float Y, float W, float H, const glm::vec3& Color);

    /// Draw multiline text. `x` is left/center/right of each line per `justify`.
    void DrawText(const std::string& Text, float X, float Y, const glm::vec3& Color,
                  float Scale = HudFontScale, ETextJustify Justify = ETextJustify::Left);

    void MeasureText(const std::string& Text, float Scale, float& OutWidth, float& OutHeight) const;

private:
    FDebugOverlay& Overlay;
    int Width = 0;
    int Height = 0;
};

