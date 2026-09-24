#include "Debug/DebugOverlay.h"
#include "GameFramework/HUD.h"
#include "Blueprint/WidgetPaintContext.h"


void HUD::Clear() {
    for (const std::unique_ptr<UserWidget>& widget : widgets_) {
        if (widget != nullptr) {
            widget->NativeDestruct();
            widget->owningHud_ = nullptr;
        }
    }
    widgets_.clear();
}

bool HUD::RemoveWidget(UserWidget* widget) {
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

void HUD::Tick(float deltaTime) {
    for (const std::unique_ptr<UserWidget>& widget : widgets_) {
        if (widget != nullptr && widget->bIsVisible) {
            widget->NativeTick(deltaTime);
        }
    }
}

void HUD::Paint(FDebugOverlay& overlay, int framebufferWidth, int framebufferHeight) {
    overlay.ClearScreenGeometry();
    if (framebufferWidth <= 0 || framebufferHeight <= 0 || widgets_.empty()) {
        return;
    }

    WidgetPaintContext ctx(overlay, framebufferWidth, framebufferHeight);
    for (const std::unique_ptr<UserWidget>& widget : widgets_) {
        if (widget != nullptr && widget->bIsVisible) {
            widget->NativePaint(ctx);
        }
    }
}

