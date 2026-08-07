#pragma once

#include <cstdint>
#include <string_view>

namespace game {

/// Wall-buy / starting weapon catalog (COD Town lite).
struct ZombiesWeaponDef {
    const char* id = "M1911";
    const char* displayName = "M1911";
    int magSize = 8;
    int startReserve = 80;
    float damage = 18.0f;
    float fireInterval = 0.18f;
    float reloadSeconds = 1.4f;
    int wallCost = 0; // 0 = not a wall buy
};

[[nodiscard]] const ZombiesWeaponDef& FindZombiesWeapon(std::string_view id);
[[nodiscard]] ZombiesWeaponDef MakePackedWeapon(const ZombiesWeaponDef& base);

} // namespace game
