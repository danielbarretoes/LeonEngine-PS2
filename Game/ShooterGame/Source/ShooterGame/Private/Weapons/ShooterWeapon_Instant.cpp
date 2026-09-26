#include "Weapons/ShooterWeapon_Instant.h"

#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterPlayerController.h"

namespace
{

	/** An impact mark's tint: a dark spot, nearly opaque. */
	const FLinearColor BulletHoleColor(0.06f, 0.055f, 0.05f, 0.92f);

} // namespace

AShooterWeapon_Instant::AShooterWeapon_Instant(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AShooterWeapon_Instant::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	WeaponRandomStream.Initialize(RandomSeed);
}

float AShooterWeapon_Instant::GetDamageAtDistance(float Distance) const
{
	const float Steps = RangeModifierDistance > 0.0f ? FMath::Max(0.0f, Distance) / RangeModifierDistance : 0.0f;
	return HitDamage * FMath::Pow(RangeModifier, Steps);
}

float AShooterWeapon_Instant::GetCurrentSpread() const
{
	float Spread = WeaponSpread + CurrentFiringSpread;
	if (MyPawn != nullptr)
	{
		const UCharacterMovementComponent& Movement = MyPawn->GetCharacterMovement();
		const float RunSpeed = FMath::Max(1.0f, Movement.MaxWalkSpeed);
		const float SpeedFraction = FMath::Clamp(Movement.Velocity.Size2D() / RunSpeed, 0.0f, 1.0f);
		Spread += MovingSpread * SpeedFraction;
		if (MyPawn->IsFalling())
		{
			Spread += JumpingSpread;
		}
		if (MyPawn->bIsCrouched)
		{
			Spread *= CrouchingSpreadMod;
		}
	}
	return Spread;
}

void AShooterWeapon_Instant::FireWeapon()
{
	FVector Start;
	FVector AimDir;
	GetAim(Start, AimDir);
	const float ConeHalfAngle = FMath::DegreesToRadians(GetCurrentSpread());
	const FVector ShootDir = WeaponRandomStream.VRandCone(AimDir, ConeHalfAngle);
	const FVector End = Start + (ShootDir * WeaponRange);

	FCollisionQueryParams TraceParams(FName(TEXT("WeaponTrace")), true, GetInstigator());
	TraceParams.AddIgnoredActor(this);
	FHitResult Impact;
	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		(void)UGameplayStatics::LineTraceSingleByChannel(*World, Impact, Start, End, COLLISION_WEAPON, TraceParams);
	}
	LastShotStart = Start;
	LastShotDirection = ShootDir;
	LastHit = Impact;
	ProcessInstantHit(Impact, Start, ShootDir);

	CurrentFiringSpread = FMath::Min(FiringSpreadMax, CurrentFiringSpread + FiringSpreadIncrement);
	ApplyRecoil();
}

void AShooterWeapon_Instant::ProcessInstantHit(const FHitResult& Impact, const FVector& Origin, const FVector& ShootDir)
{
	const FVector EndPoint = Impact.bBlockingHit ? Impact.ImpactPoint : Origin + (ShootDir * WeaponRange);
	UGameplayStatics::SpawnTracer(this, GetMuzzleLocation(), EndPoint, TracerColor, TracerWidth, TracerLifeSpan);
	if (!Impact.bBlockingHit)
	{
		return;
	}
	AActor* HitActor = Impact.GetActor();
	AShooterCharacter* HitCharacter = Cast<AShooterCharacter>(HitActor);
	if (HitCharacter == nullptr)
	{
		// A surface keeps a mark (UE ShooterGame: the impact effect's decal).
		(void)UGameplayStatics::SpawnImpactMark(
			this, Impact.ImpactPoint, Impact.ImpactNormal, ImpactMarkSize, BulletHoleColor);
	}
	if (HitActor == nullptr || !HitActor->CanBeDamaged())
	{
		return;
	}
	const float Distance = FVector::Dist(Origin, Impact.ImpactPoint);
	const bool bWasAlive = HitCharacter != nullptr && HitCharacter->IsAlive();
	const float Taken = UGameplayStatics::ApplyPointDamage(HitActor, GetDamageAtDistance(Distance), ShootDir, Impact,
		GetInstigatorController(), this, UDamageType::StaticClass());
	// The shooter's hit marker (CS: the hit sound; UE ShooterGame: the HUD's hit notify).
	AShooterPlayerController* Shooter = Cast<AShooterPlayerController>(GetInstigatorController());
	if (Shooter != nullptr && HitCharacter != nullptr && Taken > 0.0f)
	{
		Shooter->NotifyHitConfirmed(HitCharacter->GetHitGroup(Impact.ImpactPoint) == EShooterHitGroup::Head,
			bWasAlive && !HitCharacter->IsAlive());
	}
}

void AShooterWeapon_Instant::ApplyRecoil()
{
	const float PitchKick = RecoilPitch + WeaponRandomStream.FRandRange(-RecoilPitchRandom, RecoilPitchRandom);
	const float YawKick = WeaponRandomStream.FRandRange(-RecoilYawRandom, RecoilYawRandom);
	AController* Controller = GetInstigatorController();
	if (Controller == nullptr)
	{
		return;
	}
	FRotator Aim = Controller->GetControlRotation();
	const float NewPitch = FMath::Clamp(Aim.Pitch + PitchKick, -89.0f, 89.0f);
	RecoilPitchToRecover += NewPitch - Aim.Pitch;
	Aim.Pitch = NewPitch;
	Aim.Yaw += YawKick;
	Controller->SetControlRotation(Aim);
}

void AShooterWeapon_Instant::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bWantsToFire && CanFire())
	{
		return;
	}
	// The trigger is released: the accuracy and the aim come back.
	CurrentFiringSpread = FMath::Max(0.0f, CurrentFiringSpread - (FiringSpreadRecovery * DeltaSeconds));
	if (RecoilPitchToRecover > 0.0f)
	{
		const float Recovered = FMath::Min(RecoilPitchToRecover, RecoilRecovery * DeltaSeconds);
		RecoilPitchToRecover -= Recovered;
		if (AController* Controller = GetInstigatorController())
		{
			FRotator Aim = Controller->GetControlRotation();
			Aim.Pitch -= Recovered;
			Controller->SetControlRotation(Aim);
		}
	}
}

AShooterWeapon_Pistol::AShooterWeapon_Pistol(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CS's USP; DefaultGame.ini's [/Script/ShooterGame.ShooterWeapon_Pistol] tunes it.
	WeaponName = TEXT("usp");
	Slot = EShooterWeaponSlot::Secondary;
	bAutomatic = false;
	AmmoPerClip = 12;
	MaxAmmo = 100;
	TimeBetweenShots = 0.15f;
	ReloadDuration = 2.7f;
	EquipDuration = 1.0f;
	HitDamage = 34.0f;
	RangeModifier = 0.79f;
	ArmorRatio = 1.0f;
	Price = 500;
}

AShooterWeapon_Rifle::AShooterWeapon_Rifle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// CS's AK-47; DefaultGame.ini's [/Script/ShooterGame.ShooterWeapon_Rifle] tunes it.
	WeaponName = TEXT("ak47");
	Slot = EShooterWeaponSlot::Primary;
	bAutomatic = true;
	AmmoPerClip = 30;
	MaxAmmo = 90;
	TimeBetweenShots = 0.1f;
	ReloadDuration = 2.5f;
	EquipDuration = 1.0f;
	HitDamage = 36.0f;
	RangeModifier = 0.98f;
	ArmorRatio = 1.55f;
	Price = 2500;
}
