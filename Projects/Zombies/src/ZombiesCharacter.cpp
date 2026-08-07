#include "ZombiesCharacter.h"

#include <algorithm>
#include <leon/gameplay/SkeletalMeshComponent.h>

namespace game {

ZombiesCharacter::ZombiesCharacter() {
    leon::CapsuleShape capsule{};
    capsule.radius = 0.35f;
    capsule.height = 1.85f;
    SetCapsule(capsule);

    leon::CharacterMovement movement{};
    movement.MaxWalkSpeed = 4.5f;
    movement.WalkBounds = 40.0f;
    movement.ModelYawOffsetDegrees = 0.0f;
    movement.TurnSharpness = 16.0f;
    movement.JumpZVelocity = 7.0f;
    movement.Gravity = 24.0f;
    movement.FloorY = 0.0f;
    movement.MaxStepHeight = 0.35f;
    movement.PushStrength = 0.85f;
    SetCharacterMovement(movement);

    springArm_.SetOwner(this);
    (void)springArm_.AttachToComponent(&GetRootComponent());
    springArm_.TargetArmLength = 0.0f;
    springArm_.ArmLengthMin = 0.0f;
    springArm_.ArmLengthMax = 0.0f;
    springArm_.SocketOffsetZ = 1.6f;
    springArm_.BoomYawDegrees = 0.0f;
    springArm_.BoomPitchDegrees = 0.0f;
    springArm_.bEnableCameraLag = false;
    springArm_.bEnableCameraRotationLag = false;
    springArm_.bDoCollisionTest = false;

    ApplyWeapon(FindZombiesWeapon("M1911"), false);
}

void ZombiesCharacter::ApplyWeapon(const ZombiesWeaponDef& def, bool packed) {
    ZombiesWeaponDef use = packed ? MakePackedWeapon(def) : def;
    weaponId_ = def.id;
    weaponName_ = packed ? (std::string(def.displayName) + " PaP") : def.displayName;
    bPacked_ = packed;
    weaponDamage_ = use.damage;
    fireInterval_ = use.fireInterval;
    reloadSeconds_ = use.reloadSeconds;
    magSize_ = use.magSize;
    ammoInMag_ = magSize_;
    ammoReserve_ = use.startReserve;
    reloadRemaining_ = 0.0f;
}

void ZombiesCharacter::PackAPunchCurrentWeapon() {
    ApplyWeapon(FindZombiesWeapon(weaponId_), true);
}

void ZombiesCharacter::RefillAmmoFull() {
    ammoInMag_ = magSize_;
    ammoReserve_ = (std::max)(ammoReserve_, magSize_ * 8);
    reloadRemaining_ = 0.0f;
}

void ZombiesCharacter::ResetAmmo() {
    ApplyWeapon(FindZombiesWeapon(weaponId_.empty() ? "M1911" : weaponId_), bPacked_);
}

void ZombiesCharacter::SetAmmo(int inMag, int reserve) {
    const int mag = std::clamp(inMag, 0, magSize_);
    const int res = std::max(0, reserve);
    if (reloadRemaining_ > 0.0f && mag == ammoInMag_ && res == ammoReserve_) {
        return;
    }
    if (reloadRemaining_ > 0.0f) {
        reloadRemaining_ = 0.0f;
    }
    ammoInMag_ = mag;
    ammoReserve_ = res;
}

void ZombiesCharacter::ReconcileAmmoFromAuthority(int snapMag, int snapReserve,
                                                  bool allowHardSync) {
    const int mag = std::clamp(snapMag, 0, magSize_);
    const int res = std::max(0, snapReserve);

    if (allowHardSync) {
        reloadRemaining_ = 0.0f;
        ammoInMag_ = mag;
        ammoReserve_ = res;
        return;
    }

    if (reloadRemaining_ > 0.0f) {
        if (mag == ammoInMag_ && res == ammoReserve_) {
            return;
        }
        reloadRemaining_ = 0.0f;
        ammoInMag_ = mag;
        ammoReserve_ = res;
        return;
    }

    if (mag > ammoInMag_ && res < ammoReserve_) {
        ammoInMag_ = mag;
        ammoReserve_ = res;
        return;
    }
    if (mag == ammoInMag_) {
        ammoReserve_ = res;
    }
}

bool ZombiesCharacter::TryConsumeShot() {
    if (!IsAlive() || reloadRemaining_ > 0.0f || ammoInMag_ <= 0) {
        return false;
    }
    --ammoInMag_;
    return true;
}

bool ZombiesCharacter::BeginReload() {
    if (!IsAlive() || reloadRemaining_ > 0.0f) {
        return false;
    }
    if (ammoInMag_ >= magSize_ || ammoReserve_ <= 0) {
        return false;
    }
    reloadRemaining_ = GetEffectiveReloadSeconds();
    return true;
}

void ZombiesCharacter::finishReload() {
    const int need = magSize_ - ammoInMag_;
    const int take = std::min(need, ammoReserve_);
    ammoInMag_ += take;
    ammoReserve_ -= take;
    reloadRemaining_ = 0.0f;
}

void ZombiesCharacter::TickWeapon(float deltaTime) {
    if (reloadRemaining_ <= 0.0f) {
        return;
    }
    reloadRemaining_ -= deltaTime;
    if (reloadRemaining_ <= 0.0f) {
        finishReload();
    }
}

void ZombiesCharacter::SetHideMeshForLocal(bool hide) {
    if (hide == bMeshHidden_) {
        return;
    }
    bMeshHidden_ = hide;
    leon::SkeletalMeshComponent& mesh = GetMesh();
    if (hide) {
        meshScaleBeforeHide_ = mesh.RelativeScale;
        mesh.RelativeScale = {0.0f, 0.0f, 0.0f};
    } else {
        mesh.RelativeScale = meshScaleBeforeHide_;
    }
}

} // namespace game
