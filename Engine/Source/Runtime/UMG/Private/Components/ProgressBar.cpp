#include "Components/ProgressBar.h"

#include <cstdio>
#include <string>


void UProgressBar::NativePaint(FPaintContext& Ctx) {
    if (!IsVisible()) {
        return;
    }

    float LocalX = X;
    float LocalY = Y;
    float LocalW = W;
    float LocalH = H;
    if (bAnchoredBottomCenter) {
        LocalW = std::min(W, static_cast<float>(Ctx.GetWidth()) * 0.55f);
        LocalH = H;
        LocalX = (static_cast<float>(Ctx.GetWidth()) - LocalW) * 0.5f;
        LocalY = static_cast<float>(Ctx.GetHeight()) - LocalH - 36.0f;
    }

    Ctx.DrawRect(LocalX, LocalY, LocalW, LocalH, BackgroundColor);
    const float FillW = LocalW * Percent;
    if (FillW > 0.5f) {
        Ctx.DrawRect(LocalX, LocalY, FillW, LocalH, FillColor);
    }
    // 1px border via edge strips.
    Ctx.DrawRect(LocalX, LocalY, LocalW, 1.0f, BorderColor);
    Ctx.DrawRect(LocalX, LocalY + LocalH - 1.0f, LocalW, 1.0f, BorderColor);
    Ctx.DrawRect(LocalX, LocalY, 1.0f, LocalH, BorderColor);
    Ctx.DrawRect(LocalX + LocalW - 1.0f, LocalY, 1.0f, LocalH, BorderColor);

    if (bShowPercentText) {
        char Buf[16];
        std::snprintf(Buf, sizeof(Buf), "%d%%", static_cast<int>(Percent * 100.0f + 0.5f));
        float Tw = 0.0f;
        float Th = 0.0f;
        Ctx.MeasureText(Buf, HudFontScale, Tw, Th);
        Ctx.DrawText(Buf, LocalX + LocalW * 0.5f, LocalY + (LocalH - Th) * 0.5f, TextColor, HudFontScale,
                     ETextJustify::Center);
    }
}

