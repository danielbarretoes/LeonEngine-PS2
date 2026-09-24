#include "Components/InteractionPromptWidget.h"

#include <glm/vec3.hpp>

namespace {

void DrawOutlinedText(FPaintContext& Ctx, const std::string& Text, float X, float Y,
                      const glm::vec3& Color, float Scale, ETextJustify Justify) {
    constexpr glm::vec3 Shadow{0.02f, 0.02f, 0.02f};
    Ctx.DrawText(Text, X + 2.0f, Y + 2.0f, Shadow, Scale, Justify);
    Ctx.DrawText(Text, X, Y, Color, Scale, Justify);
}

} // namespace

void UInteractionPromptWidget::NativePaint(FPaintContext& Ctx) {
    if (Prompt.empty()) {
        return;
    }
    const float X = static_cast<float>(Ctx.GetWidth()) * 0.5f;
    const float Y = static_cast<float>(Ctx.GetHeight()) * NormalizedY;
    DrawOutlinedText(Ctx, Prompt, X, Y, Color, Scale, Justify);
}

