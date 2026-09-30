#include "Weapons/ShooterWeapon_AWP.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ShooterCharacter.h"

AShooterWeapon_AWP::AShooterWeapon_AWP(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CS's AWP; DefaultGame.ini's [/Script/ShooterGame.ShooterWeapon_AWP] tunes it.
	WeaponName = TEXT("awp");
	DisplayName = TEXT("AWP");
	Slot = EShooterWeaponSlot::Primary;
	bAutomatic = false;
	AmmoPerClip = 10;
	MaxAmmo = 30;
	AmmoBoxRounds = 10;
	AmmoBoxPrice = 125;
	TimeBetweenShots = 1.45f;
	ReloadDuration = 3.7f;
	EquipDuration = 1.25f;
	HitDamage = 115.0f;
	RangeModifier = 0.99f;
	ArmorRatio = 1.95f;
	SpeedModifier = 0.84f;
	// CS's AWPPrimaryAttack: the air 0.85, past 140 units a second 0.25, walking 0.1, ducking 0, still 0.001.
	WeaponSpread = 0.05f;
	WalkingSpread = 1.2f;
	MovingSpread = 3.0f;
	JumpingSpread = 6.0f;
	CrouchingSpreadMod = 0.5f;
	Price = 4750;
	// .338 Magnum: 45 units of power, two walls, 8000 units.
	PenetrationCount = 3;
	PenetrationPower = 114.3f;
	PenetrationDistance = 20320.0f;
	// CS: 40 and 10 degrees wide at 4:3 are 30.5 and 7.5 degrees high.
	ZoomFOVs.Add(30.5f);
	ZoomFOVs.Add(7.5f);
}

void AShooterWeapon_AWP::StartSecondaryFire()
{
	if (!IsEquipped() || CurrentState == EShooterWeaponState::Reloading)
	{
		return;
	}
	SetZoomLevel(ZoomLevel >= ZoomFOVs.Num() ? 0 : ZoomLevel + 1);
	ResumeZoomLevel = 0;
}

void AShooterWeapon_AWP::SetZoomLevel(int32 NewZoomLevel)
{
	ZoomLevel = FMath::Clamp(NewZoomLevel, 0, ZoomFOVs.Num());
	if (MyPawn == nullptr)
	{
		return;
	}
	// The eye zooms; the weapon itself is not in the scope's view.
	const float FieldOfView = ZoomLevel > 0 ? ZoomFOVs[ZoomLevel - 1] : MyPawn->GetDefaultFieldOfView();
	MyPawn->GetFirstPersonCameraComponent()->SetFieldOfView(FieldOfView);
	Mesh1P->SetVisibility(IsEquipped() && ZoomLevel == 0);
}

float AShooterWeapon_AWP::GetCurrentSpread() const
{
	return Super::GetCurrentSpread() + (IsZoomed() ? 0.0f : UnscopedSpread);
}

float AShooterWeapon_AWP::GetSpeedModifier() const
{
	return IsZoomed() ? ScopedSpeedModifier : SpeedModifier;
}

void AShooterWeapon_AWP::OnShotFired()
{
	Super::OnShotFired();
	if (IsZoomed())
	{
		ResumeZoomLevel = ZoomLevel;
		ResumeZoomTime = LastFireTime + TimeBetweenShots;
		SetZoomLevel(0);
	}
}

void AShooterWeapon_AWP::StartReload()
{
	Super::StartReload();
	// CS: the reload leaves the scope, and the bolt's zoom does not come back.
	if (CurrentState == EShooterWeaponState::Reloading)
	{
		SetZoomLevel(0);
		ResumeZoomLevel = 0;
	}
}

void AShooterWeapon_AWP::OnUnEquip()
{
	SetZoomLevel(0);
	ResumeZoomLevel = 0;
	Super::OnUnEquip();
}

void AShooterWeapon_AWP::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ResumeZoomLevel > 0 && GetWorldTime() >= ResumeZoomTime)
	{
		if (IsEquipped() && CurrentState != EShooterWeaponState::Reloading)
		{
			SetZoomLevel(ResumeZoomLevel);
		}
		ResumeZoomLevel = 0;
	}
}
