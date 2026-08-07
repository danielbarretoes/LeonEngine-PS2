#include "ZombiesWeapon.h"

#include <algorithm>

namespace game {
namespace {

constexpr ZombiesWeaponDef kWeapons[] = {
    {"M1911", "M1911", 8, 80, 18.0f, 0.18f, 1.4f, 0},
    {"M14", "M14", 8, 96, 32.0f, 0.22f, 1.7f, 500},
    {"MP5", "MP5", 30, 120, 22.0f, 0.09f, 1.9f, 1000},
    {"M16", "M16", 30, 150, 28.0f, 0.11f, 2.0f, 1200},
    {"Olympia", "Olympia", 2, 24, 90.0f, 0.55f, 2.4f, 500},
};

} // namespace

const ZombiesWeaponDef& FindZombiesWeapon(std::string_view id) {
    for (const ZombiesWeaponDef& w : kWeapons) {
        if (id == w.id) {
            return w;
        }
    }
    return kWeapons[0];
}

ZombiesWeaponDef MakePackedWeapon(const ZombiesWeaponDef& base) {
    ZombiesWeaponDef pap = base;
    pap.damage *= 2.2f;
    pap.magSize = (std::max)(base.magSize + 8, static_cast<int>(base.magSize * 1.5f));
    pap.startReserve = base.startReserve + 60;
    pap.reloadSeconds *= 0.85f;
    pap.fireInterval *= 0.9f;
    return pap;
}

} // namespace game
