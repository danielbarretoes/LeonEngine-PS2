#pragma once

#include <glm/vec3.hpp>
#include "Blueprint/UserWidget.h"


/// Unreal-like UImage (lite): solid tinted rect (no texture brush yet — HUD DrawRect only).
/// Useful as panel chrome, health backdrop, letterbox bars.
class ImageWidget : public UserWidget {
public:
    void SetPosition(float x, float y) {
        x_ = x;
        y_ = y;
    }
    void SetSize(float w, float h) {
        w_ = w;
        h_ = h;
    }

    [[nodiscard]] float GetX() const { return x_; }
    [[nodiscard]] float GetY() const { return y_; }
    [[nodiscard]] float GetWidth() const { return w_; }
    [[nodiscard]] float GetHeight() const { return h_; }

    void SetColor(const glm::vec3& color) { color_ = color; }
    [[nodiscard]] const glm::vec3& GetColor() const { return color_; }

    void SetBorderColor(const glm::vec3& color) { borderColor_ = color; }
    void SetDrawBorder(bool enabled) { drawBorder_ = enabled; }

    /// Stretch to full framebuffer each paint (dim overlay / letterbox).
    void SetFillScreen(bool enabled) { fillScreen_ = enabled; }

    void NativePaint(WidgetPaintContext& ctx) override;

private:
    float x_ = 0.0f;
    float y_ = 0.0f;
    float w_ = 64.0f;
    float h_ = 64.0f;
    bool drawBorder_ = false;
    bool fillScreen_ = false;
    glm::vec3 color_{0.08f, 0.08f, 0.10f};
    glm::vec3 borderColor_{0.45f, 0.38f, 0.22f};
};

