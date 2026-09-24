#include "Debug/DebugOverlay.h"
#include "GameFramework/HUD.h"
#include "Blueprint/PaintContext.h"


void AHUD::Clear() {
    for (const std::unique_ptr<UUserWidget>& widget : widgets_) {
        if (widget != nullptr) {
            widget->NativeDestruct();
            widget->owningHud_ = nullptr;
        }
    }
    widgets_.clear();
}

bool AHUD::RemoveWidget(UUserWidget* widget) {
    if (widget == nullptr) {
        return false;
    }
    for (auto it = widgets_.begin(); it != widgets_.end(); ++it) {
        if (it->get() == widget) {
            (*it)->NativeDestruct();
            (*it)->owningHud_ = nullptr;
            widgets_.erase(it);
            return true;
        }
    }
    return false;
}

void AHUD::Tick(float deltaTime) {
    for (const std::unique_ptr<UUserWidget>& widget : widgets_) {
        if (widget != nullptr && widget->bIsVisible) {
            widget->NativeTick(deltaTime);
        }
    }
}

void AHUD::Paint(FDebugOverlay& overlay, int framebufferWidth, int framebufferHeight) {
    overlay.ClearScreenGeometry();
    if (framebufferWidth <= 0 || framebufferHeight <= 0 || widgets_.empty()) {
        return;
    }

    FPaintContext ctx(overlay, framebufferWidth, framebufferHeight);
    for (const std::unique_ptr<UUserWidget>& widget : widgets_) {
        if (widget != nullptr && widget->bIsVisible) {
            widget->NativePaint(ctx);
        }
    }
}

