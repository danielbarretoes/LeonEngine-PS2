#pragma once

#include <glm/vec3.hpp>
#include "Fonts/TextLayout.h"
#include "Blueprint/UserWidget.h"
#include <algorithm>


/// Unreal-like UProgressBar (lite): background + fill rect, optional percent label.
class ProgressBarWidget : public UserWidget {
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

    /// Normalized fill amount in [0, 1].
    void SetPercent(float percent) { percent_ = std::clamp(percent, 0.0f, 1.0f); }
    [[nodiscard]] float GetPercent() const { return percent_; }

    void SetShowPercentText(bool show) { showPercentText_ = show; }
    void SetBackgroundColor(const glm::vec3& color) { backgroundColor_ = color; }
    void SetFillColor(const glm::vec3& color) { fillColor_ = color; }
    void SetBorderColor(const glm::vec3& color) { borderColor_ = color; }
    void SetTextColor(const glm::vec3& color) { textColor_ = color; }

    /// Place horizontally centered near the bottom of the viewport each paint.
    void SetAnchoredBottomCenter(bool enabled) { anchoredBottomCenter_ = enabled; }

    void NativePaint(WidgetPaintContext& ctx) override;

private:
    float x_ = 0.0f;
    float y_ = 0.0f;
    float w_ = 280.0f;
    float h_ = 18.0f;
    float percent_ = 0.0f;
    bool showPercentText_ = false;
    bool anchoredBottomCenter_ = false;

    glm::vec3 backgroundColor_{0.10f, 0.10f, 0.12f};
    glm::vec3 fillColor_{0.85f, 0.65f, 0.20f};
    glm::vec3 borderColor_{0.35f, 0.30f, 0.18f};
    glm::vec3 textColor_{1.0f, 0.92f, 0.75f};
};

