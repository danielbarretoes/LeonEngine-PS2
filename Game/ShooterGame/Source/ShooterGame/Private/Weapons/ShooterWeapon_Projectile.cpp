#include "Weapons/ShooterWeapon_Projectile.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "Sound/SoundWave.h"
#include "Weapons/ShooterProjectile.h"

namespace
{

	/** How far ahead of the eyes a projectile starts, cm (clear of the thrower's capsule). */
	constexpr float ThrowStartDistance = 50.0f;

} // namespace

AShooterWeapon_Projectile::AShooterWeapon_Projectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProjectileClass = AShooterProjectile::StaticClass();
}

void AShooterWeapon_Projectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (!ExplodeSoundName.IsNull() && FPackageName::DoesPackageExist(ExplodeSoundName.GetLongPackageName()))
	{
		ExplodeSound = Cast<USoundWave>(ExplodeSoundName.TryLoad());
	}
}

void AShooterWeapon_Projectile::FireWeapon()
{
	UWorld* World = GetWorld();
	if (World == nullptr || ProjectileClass == nullptr)
	{
		return;
	}
	FVector Start;
	FVector AimDir;
	GetAim(Start, AimDir);
	FRotator ThrowRotation = AimDir.Rotation();
	ThrowRotation.Pitch = FMath::Clamp(ThrowRotation.Pitch + ThrowPitch, -89.0f, 89.0f);
	const FVector ThrowDir = ThrowRotation.Vector();
	FVector Velocity = ThrowDir * ThrowSpeed;
	if (MyPawn != nullptr)
	{
		Velocity += MyPawn->GetCharacterMovement().Velocity;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = MyPawn;
	SpawnInfo.ObjectFlags |= RF_Transient;
	AShooterProjectile* Projectile = World->SpawnActor<AShooterProjectile>(
		ProjectileClass, Start + (ThrowDir * ThrowStartDistance), ThrowRotation, SpawnInfo);
	if (Projectile == nullptr)
	{
		return;
	}
	Projectile->FuseTime = FuseTime;
	Projectile->ExplosionDamage = ExplosionDamage;
	Projectile->ExplosionRadius = ExplosionRadius;
	Projectile->ArmorRatio = ArmorRatio;
	Projectile->ExplodeSound = ExplodeSound;
	Projectile->Launch(Velocity, GetInstigatorController(), GetWeaponMesh());
	LastProjectile = Projectile;
}

void AShooterWeapon_Projectile::SimulateWeaponFire()
{
	// A throw has no muzzle flash: only the sound.
	PlayWeaponSound(FireSound);
}

void AShooterWeapon_Projectile::OnShotFired()
{
	Super::OnShotFired();
	// HandleFiring took the round before this: nothing left means the last one is gone.
	if (CurrentAmmoInClip == 0 && CurrentAmmo == 0 && MyPawn != nullptr)
	{
		MyPawn->RemoveWeapon(this);
		(void)Destroy();
	}
}

AShooterWeapon_Grenade::AShooterWeapon_Grenade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CS's HE grenade; DefaultGame.ini's [/Script/ShooterGame.ShooterWeapon_Grenade] tunes it.
	WeaponName = TEXT("hegrenade");
	Slot = EShooterWeaponSlot::Grenade;
	bAutomatic = false;
	AmmoPerClip = 1;
	MaxAmmo = 0;
	TimeBetweenShots = 1.0f;
	ReloadDuration = 0.0f;
	EquipDuration = 0.5f;
	HeadshotMultiplier = 1.0f;
	ArmorRatio = 1.0f;
	Price = 300;
	KillReward = 300;
}
