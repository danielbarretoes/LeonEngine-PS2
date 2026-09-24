#pragma once

#include "Blueprint/WidgetPaintContext.h"


class HUD;

/// Unreal-like UUserWidget: pack HUD elements override NativePaint / NativeTick.
class UserWidget {
public:
    virtual ~UserWidget() = default;

    bool bIsVisible = true;

    virtual void NativeConstruct() {}
    virtual void NativeTick(float /*deltaTime*/) {}
    virtual void NativePaint(WidgetPaintContext& /*ctx*/) {}
    virtual void NativeDestruct() {}

    void SetVisibility(bool visible) { bIsVisible = visible; }
    [[nodiscard]] bool IsVisible() const { return bIsVisible; }

    /// Owning AHUD (set by HUD::AddWidget). Null if not added.
    [[nodiscard]] HUD* GetOwningHUD() const { return owningHud_; }

private:
    friend class HUD;
    HUD* owningHud_ = nullptr;
};

