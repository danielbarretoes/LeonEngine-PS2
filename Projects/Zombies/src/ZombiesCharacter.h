#pragma once

#include <cstdint>
#include <string>
#include <leon/Gameplay.h>
#include <leon/gameplay/SpringArmComponent.h>

#include "ZombiesWeapon.h"

namespace game {

/// Zombies Character -- zero-length boom (true first-person). Wall-buy / PaP weapons + perks.
class ZombiesCharacter final : public leon::Character {
public:
    ZombiesCharacter();

    [[nodiscard]] leon::SpringArmComponent& SpringArm() { return springArm_; }
    [[nodiscard]] const leon::SpringArmComponent& SpringArm() const { return springArm_; }

    [[nodiscard]] int GetAmmoInMag() const { return ammoInMag_; }
    [[nodiscard]] int GetAmmoReserve() const { return ammoReserve_; }
    [[nodiscard]] int GetMagSize() const { return magSize_; }
    [[nodiscard]] bool IsReloading() const { return reloadRemaining_ > 0.0f; }
    [[nodiscard]] const char* GetWeaponName() const { return weaponName_.c_str(); }
    [[nodiscard]] const std::string& GetWeaponId() const { return weaponId_; }
    [[nodiscard]] bool IsWeaponPacked() const { return bPacked_; }
    [[nodiscard]] float GetWeaponDamage() const { return weaponDamage_; }
    [[nodiscard]] float GetFireInterval() const { return fireInterval_; }
    [[nodiscard]] float GetReloadSeconds() const { return reloadSeconds_; }

    void ApplyWeapon(const ZombiesWeaponDef& def, bool packed);
    void PackAPunchCurrentWeapon();
    void RefillAmmoFull();

    void ResetAmmo();
    void SetAmmo(int inMag, int reserve);
    void ReconcileAmmoFromAuthority(int snapMag, int snapReserve, bool allowHardSync);
    [[nodiscard]] bool TryConsumeShot();
    bool BeginReload();
    void TickWeapon(float deltaTime);

    void SetHideMeshForLocal(bool hide);
    [[nodiscard]] bool IsMeshHiddenForLocal() const { return bMeshHidden_; }

    void SetReloadSpeedScale(float scale) { reloadSpeedScale_ = scale > 0.05f ? scale : 1.0f; }
    void SetFireRateScale(float scale) { fireRateScale_ = scale > 0.05f ? scale : 1.0f; }
    [[nodiscard]] float GetEffectiveFireInterval() const {
        return fireInterval_ / fireRateScale_;
    }
    [[nodiscard]] float GetEffectiveReloadSeconds() const {
        return reloadSeconds_ / reloadSpeedScale_;
    }

private:
    void finishReload();

    leon::SpringArmComponent springArm_{};
    bool bMeshHidden_ = false;
    glm::vec3 meshScaleBeforeHide_{1.0f};

    std::string weaponId_ = "M1911";
    std::string weaponName_ = "M1911";
    bool bPacked_ = false;
    float weaponDamage_ = 18.0f;
    float fireInterval_ = 0.18f;
    float reloadSeconds_ = 1.4f;
    float reloadSpeedScale_ = 1.0f;
    float fireRateScale_ = 1.0f;

    int magSize_ = 8;
    int ammoInMag_ = 8;
    int ammoReserve_ = 80;
    float reloadRemaining_ = 0.0f;
};

} // namespace game
