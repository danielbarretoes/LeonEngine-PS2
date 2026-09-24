#pragma once

#include <glm/vec3.hpp>
#include "Fonts/TextLayout.h"
#include "Blueprint/UserWidget.h"
#include <string>


/// Unreal-like UTextBlock: simple screen text (status lines, titles).
class UTextBlock : public UUserWidget {
public:
    void SetText(std::string text) { text_ = std::move(text); }
    [[nodiscard]] const std::string& GetText() const { return text_; }

    void SetColor(const glm::vec3& color) { color_ = color; }
    void SetScale(float scale) { scale_ = scale; }
    void SetJustify(ETextJustify justify) { justify_ = justify; }

    /// Anchor in pixels (top-left origin). For Center justify, X is screen center of each line.
    void SetPosition(float x, float y) {
        x_ = x;
        y_ = y;
    }

    /// Place block in the middle of the viewport (updates each paint from ctx size).
    void SetCenteredOnScreen(bool enabled) { centeredOnScreen_ = enabled; }

    void NativePaint(FPaintContext& ctx) override;

private:
    std::string text_;
    glm::vec3 color_{1.0f, 0.82f, 0.35f};
    float scale_ = HudFontScale;
    ETextJustify justify_ = ETextJustify::Center;
    float x_ = 0.0f;
    float y_ = 0.0f;
    bool centeredOnScreen_ = true;
};

