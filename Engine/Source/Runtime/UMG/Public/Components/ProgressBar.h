#pragma once

#include <glm/vec3.hpp>
#include "Fonts/TextLayout.h"
#include "Blueprint/UserWidget.h"
#include <algorithm>


/// Unreal-like UProgressBar (lite): background + fill rect, optional percent label.
class UProgressBar : public UUserWidget {
public:
    void SetPosition(float InX, float InY) {
        X = InX;
        Y = InY;
    }
    void SetSize(float InW, float InH) {
        W = InW;
        H = InH;
    }

    [[nodiscard]] float GetX() const { return X; }
    [[nodiscard]] float GetY() const { return Y; }
    [[nodiscard]] float GetWidth() const { return W; }
    [[nodiscard]] float GetHeight() const { return H; }

    /// Normalized fill amount in [0, 1].
    void SetPercent(float InPercent) { Percent = std::clamp(InPercent, 0.0f, 1.0f); }
    [[nodiscard]] float GetPercent() const { return Percent; }

    void SetShowPercentText(bool bShow) { bShowPercentText = bShow; }
    void SetBackgroundColor(const glm::vec3& Color) { BackgroundColor = Color; }
    void SetFillColor(const glm::vec3& Color) { FillColor = Color; }
    void SetBorderColor(const glm::vec3& Color) { BorderColor = Color; }
    void SetTextColor(const glm::vec3& Color) { TextColor = Color; }

    /// Place horizontally centered near the bottom of the viewport each paint.
    void SetAnchoredBottomCenter(bool bEnabled) { bAnchoredBottomCenter = bEnabled; }

    void NativePaint(FPaintContext& Ctx) override;

private:
    float X = 0.0f;
    float Y = 0.0f;
    float W = 280.0f;
    float H = 18.0f;
    float Percent = 0.0f;
    bool bShowPercentText = false;
    bool bAnchoredBottomCenter = false;

    glm::vec3 BackgroundColor{0.10f, 0.10f, 0.12f};
    glm::vec3 FillColor{0.85f, 0.65f, 0.20f};
    glm::vec3 BorderColor{0.35f, 0.30f, 0.18f};
    glm::vec3 TextColor{1.0f, 0.92f, 0.75f};
};

