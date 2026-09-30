#include "Weapons/ShooterWeapon_Projectile.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterGameMode.h"
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
	ExplodeSound = LoadShooterSound(ExplodeSoundName);
	// A grenade comes one at a time (CS: a flashbang is bought twice for two).
	CurrentAmmoInClip = FMath::Min(1, AmmoPerClip);
	CurrentAmmo = 0;
}

bool AShooterWeapon_Projectile::AddGrenade()
{
	if (CurrentAmmoInClip >= AmmoPerClip)
	{
		return false;
	}
	++CurrentAmmoInClip;
	return true;
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
	Projectile->WeaponName = WeaponName;
	Projectile->KillReward = KillReward;
	Projectile->Launch(Velocity, GetInstigatorController(), GetWeaponMesh());
	LastProjectile = Projectile;
	// CS: every throw calls it on the team's radio.
	if (AShooterGameMode* GameMode = World->GetAuthGameMode<AShooterGameMode>())
	{
		(void)GameMode->SendRadioMessage(GetInstigatorController(), EShooterRadioMessage::FireInTheHole);
	}
}

void AShooterWeapon_Projectile::SimulateWeaponFire()
{
	// A throw has no muzzle flash: only the sound (and the owner's pad).
	PlayWeaponSound(FireSound);
	PlayFireForceFeedback();
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

AShooterWeapon_HEGrenade::AShooterWeapon_HEGrenade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CS's HE grenade; DefaultGame.ini's [/Script/ShooterGame.ShooterWeapon_HEGrenade] tunes it.
	WeaponName = TEXT("hegrenade");
	DisplayName = TEXT("HE Grenade");
	Slot = EShooterWeaponSlot::Grenade;
	GrenadeOrder = 0;
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

AShooterWeapon_Flashbang::AShooterWeapon_Flashbang(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CS's flashbang: two a player; DefaultGame.ini's [/Script/ShooterGame.ShooterWeapon_Flashbang] tunes it.
	WeaponName = TEXT("flashbang");
	DisplayName = TEXT("Flashbang");
	Slot = EShooterWeaponSlot::Grenade;
	GrenadeOrder = 1;
	ProjectileClass = AShooterProjectile_Flashbang::StaticClass();
	bAutomatic = false;
	AmmoPerClip = 2;
	MaxAmmo = 0;
	TimeBetweenShots = 1.0f;
	ReloadDuration = 0.0f;
	EquipDuration = 0.5f;
	ExplosionDamage = 0.0f;
	HeadshotMultiplier = 1.0f;
	Price = 200;
}

AShooterWeapon_SmokeGrenade::AShooterWeapon_SmokeGrenade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CS's smoke grenade; DefaultGame.ini's [/Script/ShooterGame.ShooterWeapon_SmokeGrenade] tunes it.
	WeaponName = TEXT("smokegrenade");
	DisplayName = TEXT("Smoke Grenade");
	Slot = EShooterWeaponSlot::Grenade;
	GrenadeOrder = 2;
	ProjectileClass = AShooterProjectile_Smoke::StaticClass();
	bAutomatic = false;
	AmmoPerClip = 1;
	MaxAmmo = 0;
	TimeBetweenShots = 1.0f;
	ReloadDuration = 0.0f;
	EquipDuration = 0.5f;
	ExplosionDamage = 0.0f;
	HeadshotMultiplier = 1.0f;
	Price = 300;
}
