#pragma once

#include "Blueprint/PaintContext.h"


class AHUD;

/// Unreal-like UUserWidget: pack HUD elements override NativePaint / NativeTick.
class UUserWidget {
public:
    virtual ~UUserWidget() = default;

    bool bIsVisible = true;

    virtual void NativeConstruct() {}
    virtual void NativeTick(float /*deltaTime*/) {}
    virtual void NativePaint(FPaintContext& /*ctx*/) {}
    virtual void NativeDestruct() {}

    void SetVisibility(bool visible) { bIsVisible = visible; }
    [[nodiscard]] bool IsVisible() const { return bIsVisible; }

    /// Owning AHUD (set by HUD::AddWidget). Null if not added.
    [[nodiscard]] AHUD* GetOwningHUD() const { return owningHud_; }

private:
    friend class AHUD;
    AHUD* owningHud_ = nullptr;
};

