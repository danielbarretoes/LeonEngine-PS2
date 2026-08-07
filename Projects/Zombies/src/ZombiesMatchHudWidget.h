#pragma once

#include <leon/ui/UserWidget.h>
#include <string>

namespace game {

/// COD Zombies-style match chrome: round (top-left), points (bottom-left), ammo (bottom-right).
class ZombiesMatchHudWidget final : public leon::UserWidget {
public:
    void NativePaint(leon::WidgetPaintContext& ctx) override;

    int RoundIndex = 0;
    int ZombiesRemaining = 0;
    bool Intermission = false;
    float IntermissionSeconds = 0.0f;
    bool GameOver = false;

    int Score = 0;
    int Lives = 0;
    float Health = 100.0f;
    float MaxHealth = 100.0f;

    int AmmoInMag = 30;
    int AmmoReserve = 270;
    bool Reloading = false;
    std::string WeaponName = "M1911";
    /// Center interact hint ("[F] Open Door [750]").
    std::string InteractPrompt;
};

} // namespace game
