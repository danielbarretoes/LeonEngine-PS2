#pragma once

#include "Blueprint/PaintContext.h"


class AHUD;

/// Unreal-like UUserWidget: pack HUD elements override NativePaint / NativeTick.
class UMG_API UUserWidget {
public:
    virtual ~UUserWidget() = default;

    bool bIsVisible = true;

    virtual void NativeConstruct() {}
    virtual void NativeTick(float /*deltaTime*/) {}
    virtual void NativePaint(FPaintContext& /*ctx*/) {}
    virtual void NativeDestruct() {}

    void SetVisibility(bool bVisible) { bIsVisible = bVisible; }
    [[nodiscard]] bool IsVisible() const { return bIsVisible; }

    /// Owning AHUD (set by HUD::AddWidget). Null if not added.
    [[nodiscard]] AHUD* GetOwningHUD() const { return OwningHud; }

private:
    friend class AHUD;
    AHUD* OwningHud = nullptr;
};

