#pragma once

#include <leon/ui/UserWidget.h>

namespace game {

/// Center-screen FPS crosshair (gap + arms). Painted via HUD screen geometry.
class ZombiesCrosshairWidget final : public leon::UserWidget {
public:
    void NativePaint(leon::WidgetPaintContext& ctx) override;

    /// Dims when health is low (set each match tick).
    float Pulse = 1.0f;
};

} // namespace game
