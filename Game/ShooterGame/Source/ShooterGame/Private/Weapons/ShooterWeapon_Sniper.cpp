#include "Weapons/ShooterWeapon_Sniper.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ShooterCharacter.h"

AShooterWeapon_Sniper::AShooterWeapon_Sniper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CS's AWP; DefaultGame.ini's [/Script/ShooterGame.ShooterWeapon_Sniper] tunes it.
	WeaponName = TEXT("awp");
	Slot = EShooterWeaponSlot::Primary;
	bAutomatic = false;
	AmmoPerClip = 10;
	MaxAmmo = 30;
	TimeBetweenShots = 1.45f;
	ReloadDuration = 3.7f;
	EquipDuration = 1.25f;
	HitDamage = 115.0f;
	RangeModifier = 0.99f;
	ArmorRatio = 1.95f;
	SpeedModifier = 0.84f;
	WeaponSpread = 0.05f;
	Price = 4750;
	// CS: 40 and 10 degrees wide at 4:3 are 30.5 and 7.5 degrees high.
	ZoomFOVs.Add(30.5f);
	ZoomFOVs.Add(7.5f);
}

void AShooterWeapon_Sniper::StartSecondaryFire()
{
	if (!IsEquipped() || CurrentState == EShooterWeaponState::Reloading)
	{
		return;
	}
	SetZoomLevel(ZoomLevel >= ZoomFOVs.Num() ? 0 : ZoomLevel + 1);
	ResumeZoomLevel = 0;
}

void AShooterWeapon_Sniper::SetZoomLevel(int32 NewZoomLevel)
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

float AShooterWeapon_Sniper::GetCurrentSpread() const
{
	return Super::GetCurrentSpread() + (IsZoomed() ? 0.0f : UnscopedSpread);
}

float AShooterWeapon_Sniper::GetSpeedModifier() const
{
	return IsZoomed() ? ScopedSpeedModifier : SpeedModifier;
}

void AShooterWeapon_Sniper::OnShotFired()
{
	Super::OnShotFired();
	if (IsZoomed())
	{
		ResumeZoomLevel = ZoomLevel;
		ResumeZoomTime = LastFireTime + TimeBetweenShots;
		SetZoomLevel(0);
	}
}

void AShooterWeapon_Sniper::OnUnEquip()
{
	SetZoomLevel(0);
	ResumeZoomLevel = 0;
	Super::OnUnEquip();
}

void AShooterWeapon_Sniper::Tick(float DeltaSeconds)
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
