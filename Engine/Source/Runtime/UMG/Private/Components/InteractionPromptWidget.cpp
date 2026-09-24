#include "Components/InteractionPromptWidget.h"

#include <glm/vec3.hpp>

namespace {

void DrawOutlinedText(FPaintContext& ctx, const std::string& text, float x, float y,
                      const glm::vec3& color, float scale, ETextJustify justify) {
    constexpr glm::vec3 kShadow{0.02f, 0.02f, 0.02f};
    ctx.DrawText(text, x + 2.0f, y + 2.0f, kShadow, scale, justify);
    ctx.DrawText(text, x, y, color, scale, justify);
}

} // namespace

void UInteractionPromptWidget::NativePaint(FPaintContext& ctx) {
    if (Prompt.empty()) {
        return;
    }
    const float x = static_cast<float>(ctx.Width()) * 0.5f;
    const float y = static_cast<float>(ctx.Height()) * NormalizedY;
    DrawOutlinedText(ctx, Prompt, x, y, Color, Scale, Justify);
}

