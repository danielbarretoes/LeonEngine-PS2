#include "ZombiesMatchHudWidget.h"

#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <leon/ui/TextLayout.h>
#include <string>

namespace game {

namespace {

void DrawOutlinedText(leon::WidgetPaintContext& ctx, const std::string& text, float x, float y,
                      const glm::vec3& color, float scale, leon::ETextJustify justify) {
    constexpr glm::vec3 kShadow{0.02f, 0.02f, 0.02f};
    ctx.DrawText(text, x + 2.0f, y + 2.0f, kShadow, scale, justify);
    ctx.DrawText(text, x, y, color, scale, justify);
}

} // namespace

void ZombiesMatchHudWidget::NativePaint(leon::WidgetPaintContext& ctx) {
    const float w = static_cast<float>(ctx.Width());
    const float h = static_cast<float>(ctx.Height());
    constexpr float kMargin = 28.0f;

    // Top-left: round (COD-style).
    {
        std::string roundLine;
        if (GameOver) {
            roundLine = "GAME OVER";
        } else if (Intermission) {
            roundLine = "ROUND " + std::to_string(std::max(1, RoundIndex)) + "  —  " +
                        std::to_string(static_cast<int>(std::ceil(IntermissionSeconds))) + "s";
        } else {
            roundLine = "ROUND " + std::to_string(std::max(0, RoundIndex));
        }
        const glm::vec3 roundCol =
            GameOver ? glm::vec3{1.0f, 0.25f, 0.2f} : glm::vec3{0.95f, 0.2f, 0.15f};
        DrawOutlinedText(ctx, roundLine, kMargin, kMargin, roundCol, 3.2f,
                         leon::ETextJustify::Left);

        if (!GameOver && !Intermission) {
            DrawOutlinedText(ctx, "Zombies  " + std::to_string(ZombiesRemaining), kMargin,
                             kMargin + 42.0f, {0.85f, 0.85f, 0.8f}, 2.0f, leon::ETextJustify::Left);
        }
    }

    // Bottom-left: points + lives (classic Zombies).
    {
        const float baseY = h - kMargin - 70.0f;
        DrawOutlinedText(ctx, std::to_string(Score), kMargin, baseY, {0.95f, 0.95f, 0.9f}, 4.0f,
                         leon::ETextJustify::Left);
        DrawOutlinedText(ctx, "Lives  " + std::to_string(Lives), kMargin, baseY + 52.0f,
                         {0.7f, 0.7f, 0.65f}, 2.0f, leon::ETextJustify::Left);
    }

    // Bottom-right: weapon + mag / reserve + HP bar strip.
    {
        const float rightX = w - kMargin;
        const float ammoY = h - kMargin - 78.0f;
        DrawOutlinedText(ctx, WeaponName, rightX, ammoY - 36.0f, {0.75f, 0.75f, 0.7f}, 2.0f,
                         leon::ETextJustify::Right);

        const std::string ammoText =
            Reloading ? "RELOADING"
                      : (std::to_string(AmmoInMag) + " / " + std::to_string(AmmoReserve));
        glm::vec3 ammoCol{0.95f, 0.95f, 0.88f};
        if (Reloading) {
            ammoCol = {0.85f, 0.75f, 0.35f};
        } else if (AmmoInMag <= 0) {
            ammoCol = {1.0f, 0.35f, 0.25f};
        } else if (AmmoInMag <= 5) {
            ammoCol = {1.0f, 0.75f, 0.25f};
        }
        DrawOutlinedText(ctx, ammoText, rightX, ammoY, ammoCol, Reloading ? 3.0f : 4.2f,
                         leon::ETextJustify::Right);

        // HP bar under ammo.
        const float barW = 160.0f;
        const float barH = 10.0f;
        const float barX = rightX - barW;
        const float barY = ammoY + 52.0f;
        const float pct = std::clamp(Health / std::max(1.0f, MaxHealth), 0.0f, 1.0f);
        ctx.DrawRect(barX - 2.0f, barY - 2.0f, barW + 4.0f, barH + 4.0f, {0.05f, 0.05f, 0.05f});
        ctx.DrawRect(barX, barY, barW, barH, {0.25f, 0.12f, 0.1f});
        ctx.DrawRect(barX, barY, barW * pct, barH, {0.85f, 0.2f, 0.15f});
        DrawOutlinedText(ctx, std::to_string(static_cast<int>(Health)), barX - 8.0f, barY - 2.0f,
                         {0.9f, 0.85f, 0.8f}, 1.8f, leon::ETextJustify::Right);
    }

    if (!InteractPrompt.empty()) {
        DrawOutlinedText(ctx, InteractPrompt, w * 0.5f, h * 0.62f, {0.95f, 0.9f, 0.45f}, 2.4f,
                         leon::ETextJustify::Center);
    }
}

} // namespace game
