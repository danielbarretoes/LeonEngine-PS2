#include "Weapons/ShooterWeapon_Instant.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Physics/PhysScene.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterPlayerController.h"
#include "Sound/SoundWave.h"

namespace
{

	/** An impact mark's tint: a dark spot, nearly opaque (concrete, tile and the default). */
	const FLinearColor BulletHoleColor(0.06f, 0.055f, 0.05f, 0.92f);
	/** In dirt and sand a brown dent, in wood a dark brown splintered hole, on metal a grey dent, on glass a pale star.
	 */
	const FLinearColor DirtHoleColor(0.2f, 0.13f, 0.07f, 0.85f);
	const FLinearColor WoodHoleColor(0.09f, 0.05f, 0.025f, 0.92f);
	const FLinearColor MetalHoleColor(0.22f, 0.22f, 0.23f, 0.9f);
	const FLinearColor GlassHoleColor(0.75f, 0.8f, 0.82f, 0.7f);

	/** How far past a character's entry point a bullet goes on (CS: 42 units), and the damage and range it keeps. */
	constexpr float CharacterPenetrationDepth = 106.68f;
	constexpr float CharacterPenetrationDamageScale = 0.75f;
	constexpr float CharacterPenetrationRangeScale = 0.75f;

	/** The range a bullet keeps past a surface (CS: half of what was left). */
	constexpr float SurfacePenetrationRangeScale = 0.5f;

	/** How far past the exit the next segment starts, and how near the entry an exit counts as none, cm. */
	constexpr float ExitStep = 0.5f;
	constexpr float MinExitDepth = 0.5f;

	/** How early a burst's shot may come, seconds (as the base weapon's shots). */
	constexpr float BurstTimeTolerance = 1.0e-3f;

} // namespace

AShooterWeapon_Instant::AShooterWeapon_Instant(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AShooterWeapon_Instant::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	WeaponRandomStream.Initialize(RandomSeed);
	ImpactSoundSet.Load(ImpactSounds);
	ArmorHitSound = LoadShooterSound(ArmorHitSoundName);
	HelmetHitSound = LoadShooterSound(HelmetHitSoundName);
}

EPhysicalSurface AShooterWeapon_Instant::GetSurfaceType(const FHitResult& Hit)
{
	return UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
}

void AShooterWeapon_Instant::GetSurfacePenetration(
	EPhysicalSurface Surface, float& OutPowerScale, float& OutDamageScale)
{
	// CS 1.6's FireBullets3: the texture type scales the penetration power (for the rest of the shot) and sets the
	// damage kept past the surface (0.5 unless the type says otherwise).
	OutPowerScale = 1.0f;
	OutDamageScale = 0.5f;
	switch (Surface)
	{
		case SHOOTER_SURFACE_Concrete:
			OutPowerScale = 0.25f;
			break;
		case SHOOTER_SURFACE_Wood:
			OutDamageScale = 0.6f;
			break;
		case SHOOTER_SURFACE_Metal:
			OutPowerScale = 0.15f;
			OutDamageScale = 0.2f;
			break;
		case SHOOTER_SURFACE_Tile:
			OutPowerScale = 0.65f;
			OutDamageScale = 0.2f;
			break;
		case SHOOTER_SURFACE_Computer:
			OutPowerScale = 0.4f;
			OutDamageScale = 0.45f;
			break;
		default:
			// Dirt, glass, flesh and the default: CS's default.
			break;
	}
}

void AShooterWeapon_Instant::GetImpactMarkStyle(EPhysicalSurface Surface, FLinearColor& OutColor, float& OutSizeScale)
{
	OutColor = BulletHoleColor;
	OutSizeScale = 1.0f;
	switch (Surface)
	{
		case SHOOTER_SURFACE_Dirt:
			OutColor = DirtHoleColor;
			OutSizeScale = 1.4f;
			break;
		case SHOOTER_SURFACE_Wood:
			OutColor = WoodHoleColor;
			OutSizeScale = 1.1f;
			break;
		case SHOOTER_SURFACE_Metal:
		case SHOOTER_SURFACE_Computer:
			OutColor = MetalHoleColor;
			OutSizeScale = 0.7f;
			break;
		case SHOOTER_SURFACE_Glass:
			OutColor = GlassHoleColor;
			OutSizeScale = 1.2f;
			break;
		case SHOOTER_SURFACE_Tile:
			OutSizeScale = 0.9f;
			break;
		default:
			break;
	}
}

USoundWave* AShooterWeapon_Instant::GetImpactSound(const FHitResult& Impact, int32 Variant) const
{
	if (const AShooterCharacter* Victim = Cast<AShooterCharacter>(Impact.GetActor()))
	{
		// CS's TraceAttack: a helmet stops the head's blood, armor the body's (the legs have none).
		const EShooterHitGroup Group = Victim->GetHitGroup(Impact.ImpactPoint);
		const bool bLegs = Group == EShooterHitGroup::LeftLeg || Group == EShooterHitGroup::RightLeg;
		if (Group == EShooterHitGroup::Head && Victim->HasHelmet() && Victim->GetArmor() > 0.0f &&
			HelmetHitSound != nullptr)
		{
			return HelmetHitSound;
		}
		if (Group != EShooterHitGroup::Head && !bLegs && Victim->GetArmor() > 0.0f && ArmorHitSound != nullptr)
		{
			return ArmorHitSound;
		}
		return ImpactSoundSet.Get(SHOOTER_SURFACE_Flesh, Variant);
	}
	return ImpactSoundSet.Get(GetSurfaceType(Impact), Variant);
}

float AShooterWeapon_Instant::GetDamageAtDistance(float Distance) const
{
	const float Steps = RangeModifierDistance > 0.0f ? FMath::Max(0.0f, Distance) / RangeModifierDistance : 0.0f;
	return (bSilenced ? SilencedHitDamage : HitDamage) *
		FMath::Pow(bSilenced ? SilencedRangeModifier : RangeModifier, Steps);
}

float AShooterWeapon_Instant::GetCurrentSpread() const
{
	float Spread = WeaponSpread + CurrentFiringSpread;
	if (MyPawn != nullptr)
	{
		// The running speed with this weapon in hand (UShooterCharacterMovement::GetMaxSpeed without the walk key).
		const UCharacterMovementComponent& Movement = MyPawn->GetCharacterMovement();
		Spread += GetMovementSpread(Movement.Velocity.Size2D(), Movement.MaxWalkSpeed * GetSpeedModifier());
		// Off the floor: in the air or on a ladder (CS: not FL_ONGROUND).
		if (!MyPawn->IsMovingOnGround())
		{
			Spread += JumpingSpread;
		}
		if (MyPawn->bIsCrouched)
		{
			Spread *= CrouchingSpreadMod;
		}
	}
	if (bSilenced)
	{
		Spread *= SilencedSpreadScale;
	}
	if (bBurstMode)
	{
		Spread *= BurstSpreadScale;
	}
	return Spread;
}

float AShooterWeapon_Instant::GetMovementSpread(float Speed, float RunSpeed) const
{
	const float Walk = FMath::Max(1.0f, WalkingSpeed);
	if (Speed <= Walk)
	{
		return WalkingSpread * FMath::Max(0.0f, Speed) / Walk;
	}
	const float Run = FMath::Max(Walk + 1.0f, RunSpeed);
	return FMath::Lerp(WalkingSpread, MovingSpread, FMath::Min(1.0f, (Speed - Walk) / (Run - Walk)));
}

float AShooterWeapon_Instant::GetTimeBetweenShots() const
{
	return bBurstMode ? BurstCycleTime : TimeBetweenShots;
}

float AShooterWeapon_Instant::GetFireNoiseLoudness() const
{
	return bSilenced ? SilencedFireNoiseLoudness : FireNoiseLoudness;
}

float AShooterWeapon_Instant::GetFireVolume() const
{
	return bSilenced ? SilencedFireVolume : 1.0f;
}

void AShooterWeapon_Instant::SetSilenced(bool bNewSilenced)
{
	bSilenced = bHasSilencer && bNewSilenced;
}

void AShooterWeapon_Instant::StartSecondaryFire()
{
	if (!IsEquipped() || CurrentState == EShooterWeaponState::Reloading ||
		CurrentState == EShooterWeaponState::Equipping)
	{
		return;
	}
	if (bHasSilencer)
	{
		// CS: the silencer goes on or off with its animation; no shot meanwhile.
		StopFire();
		SetSilenced(!bSilenced);
		SetEquippingFor(SilencerDuration);
		UE_LOG(LogShooter, Log, TEXT("%s: silencer %s"), *WeaponName, bSilenced ? TEXT("on") : TEXT("off"));
	}
	else if (bHasBurstMode)
	{
		bBurstMode = !bBurstMode;
		BurstShotsLeft = 0;
		UE_LOG(LogShooter, Log, TEXT("%s: %s"), *WeaponName, bBurstMode ? TEXT("burst fire") : TEXT("semi-automatic"));
	}
}

void AShooterWeapon_Instant::FireWeapon()
{
	FVector Start;
	FVector AimDir;
	GetAim(Start, AimDir);
	const float ConeHalfAngle = FMath::DegreesToRadians(GetCurrentSpread());
	const FVector ShootDir = WeaponRandomStream.VRandCone(AimDir, ConeHalfAngle);
	LastShotStart = Start;
	LastShotDirection = ShootDir;
	LastHit = FHitResult();
	LastShotPenetrations = 0;

	FCollisionQueryParams TraceParams(FName(TEXT("WeaponTrace")), true, GetInstigator());
	TraceParams.AddIgnoredActor(this);
	// The surfaces' physical materials: their penetration, their marks and sounds.
	TraceParams.bReturnPhysicalMaterial = true;
	// CS's FireBullets3: segment after segment, each piercing what it hit while the penetrations last.
	UWorld* World = GetWorld();
	float Damage = bSilenced ? SilencedHitDamage : HitDamage;
	const float Falloff = bSilenced ? SilencedRangeModifier : RangeModifier;
	float Range = WeaponRange;
	float Power = PenetrationPower;
	int32 HitsLeft = FMath::Max(1, PenetrationCount);
	FVector SegmentStart = Start;
	FVector End = Start + (ShootDir * Range);
	while (World != nullptr && HitsLeft > 0 && Range > 0.0f)
	{
		FHitResult Impact;
		End = SegmentStart + (ShootDir * Range);
		if (!UGameplayStatics::LineTraceSingleByChannel(
				*World, Impact, SegmentStart, End, COLLISION_WEAPON, TraceParams))
		{
			break;
		}
		if (!LastHit.bBlockingHit)
		{
			LastHit = Impact;
		}
		End = Impact.ImpactPoint;
		--HitsLeft;
		const float SegmentDistance = FVector::Dist(SegmentStart, Impact.ImpactPoint);
		const float Steps = RangeModifierDistance > 0.0f ? SegmentDistance / RangeModifierDistance : 0.0f;
		Damage *= FMath::Pow(Falloff, Steps);
		if (FVector::Dist(Start, Impact.ImpactPoint) > PenetrationDistance)
		{
			HitsLeft = 0;
		}
		AActor* HitActor = Impact.GetActor();
		(void)ProcessInstantHit(Impact, ShootDir, Damage);
		if (HitsLeft == 0)
		{
			break;
		}
		if (Cast<AShooterCharacter>(HitActor) != nullptr)
		{
			// Through a player: on past the body with three quarters of the damage.
			TraceParams.AddIgnoredActor(HitActor);
			SegmentStart = Impact.ImpactPoint + (ShootDir * CharacterPenetrationDepth);
			Range = (Range - SegmentDistance) * CharacterPenetrationRangeScale;
			Damage *= CharacterPenetrationDamageScale;
		}
		else
		{
			// Through a surface: its material cuts the power down; the bullet must leave it within the power left.
			float PowerScale = 1.0f;
			float DamageScale = 1.0f;
			GetSurfacePenetration(GetSurfaceType(Impact), PowerScale, DamageScale);
			Power *= PowerScale;
			FVector Exit;
			if (!FindPenetrationExit(Impact, ShootDir, Power, TraceParams, Exit))
			{
				break;
			}
			SegmentStart = Exit + (ShootDir * ExitStep);
			Range = (Range - SegmentDistance) * SurfacePenetrationRangeScale;
			Damage *= DamageScale;
		}
		++LastShotPenetrations;
		End = SegmentStart + (ShootDir * Range);
	}
	LastShotEnd = End;
	UGameplayStatics::SpawnTracer(this, GetMuzzleLocation(), End, TracerColor, TracerWidth, TracerLifeSpan);

	CurrentFiringSpread = FMath::Min(FiringSpreadMax, CurrentFiringSpread + FiringSpreadIncrement);
	ApplyRecoil();
}

bool AShooterWeapon_Instant::FindPenetrationExit(const FHitResult& Entry, const FVector& Direction, float Depth,
	const FCollisionQueryParams& Params, FVector& OutExit) const
{
	const UWorld* World = GetWorld();
	if (World == nullptr || Depth <= MinExitDepth || Entry.BodyIndex == INDEX_NONE)
	{
		return false;
	}
	// A line from Depth ahead back to just before the entry: the first time it meets the entered body is its far side.
	// It meets it at once when that point is still inside a box, or at the entry itself when inside a closed mesh.
	const FVector Far = Entry.ImpactPoint + (Direction * Depth);
	const FVector Back = Entry.ImpactPoint - (Direction * ExitStep);
	TArray<FHitResult> Hits;
	(void)World->GetPhysicsScene().LineTraceMultiByChannel(Hits, Far, Back, COLLISION_WEAPON, Params);
	for (const FHitResult& Hit : Hits)
	{
		if (Hit.BodyIndex != Entry.BodyIndex)
		{
			continue;
		}
		if (Hit.Time <= 0.0f || FVector::DistSquared(Hit.ImpactPoint, Entry.ImpactPoint) < FMath::Square(MinExitDepth))
		{
			return false;
		}
		OutExit = Hit.ImpactPoint;
		return true;
	}
	return false;
}

float AShooterWeapon_Instant::ProcessInstantHit(const FHitResult& Impact, const FVector& ShootDir, float Damage)
{
	AActor* HitActor = Impact.GetActor();
	AShooterCharacter* HitCharacter = Cast<AShooterCharacter>(HitActor);
	// The hit's sound (UE ShooterGame: the impact effect's sound), before the damage changes the victim's armor.
	LastImpactSound = GetImpactSound(Impact, NumImpacts++);
	if (LastImpactSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LastImpactSound, Impact.ImpactPoint);
	}
	if (HitCharacter == nullptr)
	{
		// A surface keeps a mark (UE ShooterGame: the impact effect's decal), the surface's.
		FLinearColor MarkColor;
		float MarkScale = 1.0f;
		GetImpactMarkStyle(GetSurfaceType(Impact), MarkColor, MarkScale);
		(void)UGameplayStatics::SpawnImpactMark(
			this, Impact.ImpactPoint, Impact.ImpactNormal, ImpactMarkSize * MarkScale, MarkColor);
	}
	if (HitActor == nullptr || !HitActor->CanBeDamaged())
	{
		return 0.0f;
	}
	const bool bWasAlive = HitCharacter != nullptr && HitCharacter->IsAlive();
	const float Taken = UGameplayStatics::ApplyPointDamage(
		HitActor, Damage, ShootDir, Impact, GetInstigatorController(), this, UDamageType::StaticClass());
	// The shooter's hit marker (CS: the hit sound; UE ShooterGame: the HUD's hit notify).
	AShooterPlayerController* Shooter = Cast<AShooterPlayerController>(GetInstigatorController());
	if (Shooter != nullptr && HitCharacter != nullptr && Taken > 0.0f)
	{
		Shooter->NotifyHitConfirmed(HitCharacter->GetHitGroup(Impact.ImpactPoint) == EShooterHitGroup::Head,
			bWasAlive && !HitCharacter->IsAlive());
	}
	return Taken;
}

void AShooterWeapon_Instant::OnShotFired()
{
	Super::OnShotFired();
	// A burst's first shot (the press's): the rest follow BurstShotInterval apart (CS: FireRemaining).
	if (bBurstMode && BurstShotsLeft == 0)
	{
		BurstShotsLeft = FMath::Max(0, BurstShots - 1);
		NextBurstShotTime = LastFireTime + BurstShotInterval;
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

void AShooterWeapon_Instant::CompensateRecoil(float Degrees)
{
	const float Pulled = FMath::Clamp(Degrees, 0.0f, RecoilPitchToRecover);
	AController* Controller = GetInstigatorController();
	if (Pulled <= 0.0f || Controller == nullptr)
	{
		return;
	}
	RecoilPitchToRecover -= Pulled;
	FRotator Aim = Controller->GetControlRotation();
	Aim.Pitch -= Pulled;
	Controller->SetControlRotation(Aim);
}

void AShooterWeapon_Instant::ResetAim()
{
	CurrentFiringSpread = 0.0f;
	RecoilPitchToRecover = 0.0f;
	BurstShotsLeft = 0;
}

void AShooterWeapon_Instant::OnUnEquip()
{
	BurstShotsLeft = 0;
	Super::OnUnEquip();
}

void AShooterWeapon_Instant::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// A burst's other shots, whether the trigger is still held or not (CS).
	while (BurstShotsLeft > 0 && GetWorldTime() + BurstTimeTolerance >= NextBurstShotTime)
	{
		if (CurrentAmmoInClip <= 0 || !IsEquipped() || MyPawn == nullptr || !MyPawn->IsAlive() || MyPawn->IsFrozen())
		{
			BurstShotsLeft = 0;
			break;
		}
		--BurstShotsLeft;
		NextBurstShotTime += BurstShotInterval;
		FireWeapon();
		UseAmmo();
		++ShotsFired;
		SimulateWeaponFire();
	}
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

// The weapons, with Counter-Strike 1.6's values (1 unit = 2.54 cm); DefaultGame.ini's sections tune them.

AShooterWeapon_Glock::AShooterWeapon_Glock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponName = TEXT("glock");
	Slot = EShooterWeaponSlot::Secondary;
	bAutomatic = false;
	AmmoPerClip = 20;
	MaxAmmo = 120;
	AmmoBoxRounds = 30;
	AmmoBoxPrice = 20;
	TimeBetweenShots = 0.2f;
	ReloadDuration = 2.2f;
	HitDamage = 25.0f;
	RangeModifier = 0.75f;
	ArmorRatio = 1.05f;
	Price = 400;
	// 9 mm: 21 units of power, pierces nothing (a pistol's one hit), 800 units.
	PenetrationCount = 1;
	PenetrationPower = 53.34f;
	PenetrationDistance = 2032.0f;
	// CS's GLOCK18PrimaryAttack: the air, moving, ducking (three quarters of standing), still.
	WeaponSpread = 0.45f;
	WalkingSpread = 1.0f;
	MovingSpread = 3.5f;
	JumpingSpread = 7.0f;
	CrouchingSpreadMod = 0.65f;
	FiringSpreadIncrement = 0.5f;
	RecoilPitch = 0.6f;
	RecoilPitchRandom = 0.2f;
	RecoilYawRandom = 0.3f;
	// CS: three rounds 0.1 s apart, the next press 0.5 s later, five times the spread.
	bHasBurstMode = true;
	BurstShots = 3;
	BurstShotInterval = 0.1f;
	BurstCycleTime = 0.5f;
	BurstSpreadScale = 5.0f;
}

AShooterWeapon_USP::AShooterWeapon_USP(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponName = TEXT("usp");
	Slot = EShooterWeaponSlot::Secondary;
	bAutomatic = false;
	AmmoPerClip = 12;
	MaxAmmo = 100;
	AmmoBoxRounds = 12;
	AmmoBoxPrice = 25;
	TimeBetweenShots = 0.225f;
	ReloadDuration = 2.7f;
	HitDamage = 34.0f;
	RangeModifier = 0.79f;
	ArmorRatio = 1.0f;
	Price = 500;
	// .45 ACP: 15 units, pierces nothing, 500 units.
	PenetrationCount = 1;
	PenetrationPower = 38.1f;
	PenetrationDistance = 1270.0f;
	// CS: the silencer takes 3 s, and the silenced USP hits for 30.
	bHasSilencer = true;
	SilencerDuration = 3.0f;
	SilencedHitDamage = 30.0f;
	SilencedRangeModifier = 0.79f;
	SilencedSpreadScale = 1.0f;
	// CS's USPPrimaryAttack: the air, moving, ducking (0.08 against 0.1 standing), still.
	WeaponSpread = 0.3f;
	WalkingSpread = 1.0f;
	MovingSpread = 3.0f;
	JumpingSpread = 6.0f;
	CrouchingSpreadMod = 0.65f;
}

AShooterWeapon_Deagle::AShooterWeapon_Deagle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponName = TEXT("deagle");
	Slot = EShooterWeaponSlot::Secondary;
	bAutomatic = false;
	AmmoPerClip = 7;
	MaxAmmo = 35;
	AmmoBoxRounds = 7;
	AmmoBoxPrice = 40;
	TimeBetweenShots = 0.3f;
	ReloadDuration = 2.2f;
	HitDamage = 54.0f;
	RangeModifier = 0.81f;
	ArmorRatio = 1.5f;
	Price = 650;
	// .50 AE: 30 units, one wall, 1000 units.
	PenetrationCount = 2;
	PenetrationPower = 76.2f;
	PenetrationDistance = 2540.0f;
	// CS's DEAGLEPrimaryAttack: the air, moving (twice standing), ducking (0.115 against 0.13), still.
	WeaponSpread = 0.5f;
	WalkingSpread = 1.2f;
	MovingSpread = 4.0f;
	JumpingSpread = 8.0f;
	CrouchingSpreadMod = 0.65f;
	FiringSpreadIncrement = 1.2f;
	FiringSpreadMax = 5.0f;
	FiringSpreadRecovery = 5.0f;
	RecoilPitch = 2.0f;
	RecoilPitchRandom = 0.4f;
	RecoilYawRandom = 0.5f;
	RecoilRecovery = 8.0f;
}

AShooterWeapon_MP5::AShooterWeapon_MP5(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponName = TEXT("mp5");
	Slot = EShooterWeaponSlot::Primary;
	bAutomatic = true;
	AmmoPerClip = 30;
	MaxAmmo = 120;
	AmmoBoxRounds = 30;
	AmmoBoxPrice = 20;
	TimeBetweenShots = 0.075f;
	ReloadDuration = 2.63f;
	HitDamage = 26.0f;
	RangeModifier = 0.84f;
	ArmorRatio = 1.0f;
	Price = 1500;
	// 9 mm, as the Glock.
	PenetrationCount = 1;
	PenetrationPower = 53.34f;
	PenetrationDistance = 2032.0f;
	// CS's MP5NPrimaryAttack: the air or not; the SMG barely minds moving.
	WeaponSpread = 0.45f;
	WalkingSpread = 0.5f;
	MovingSpread = 1.5f;
	JumpingSpread = 5.0f;
	CrouchingSpreadMod = 0.6f;
	FiringSpreadIncrement = 0.25f;
	FiringSpreadMax = 3.5f;
	RecoilPitch = 0.45f;
	RecoilPitchRandom = 0.15f;
	RecoilYawRandom = 0.3f;
}

AShooterWeapon_AK47::AShooterWeapon_AK47(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponName = TEXT("ak47");
	Slot = EShooterWeaponSlot::Primary;
	BuyTeam = EShooterTeam::T;
	bAutomatic = true;
	AmmoPerClip = 30;
	MaxAmmo = 90;
	AmmoBoxRounds = 30;
	AmmoBoxPrice = 80;
	TimeBetweenShots = 0.0955f;
	ReloadDuration = 2.5f;
	HitDamage = 36.0f;
	RangeModifier = 0.98f;
	ArmorRatio = 1.55f;
	SpeedModifier = 0.884f;
	Price = 2500;
	// 7.62 mm: 39 units, one wall, 5000 units.
	PenetrationCount = 2;
	PenetrationPower = 99.06f;
	PenetrationDistance = 12700.0f;
	// CS's AK47PrimaryAttack: the air (0.04 + 0.4 x the accuracy), past 140 units a second (0.04 + 0.07 x), else
	// 0.0275 x; crouched, the AK tightens most.
	WeaponSpread = 0.35f;
	WalkingSpread = 1.2f;
	MovingSpread = 4.5f;
	JumpingSpread = 8.0f;
	CrouchingSpreadMod = 0.5f;
	FiringSpreadIncrement = 0.45f;
	FiringSpreadMax = 5.0f;
	RecoilPitch = 1.0f;
	RecoilPitchRandom = 0.3f;
	RecoilYawRandom = 0.5f;
}

AShooterWeapon_M4A1::AShooterWeapon_M4A1(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponName = TEXT("m4a1");
	Slot = EShooterWeaponSlot::Primary;
	BuyTeam = EShooterTeam::CT;
	bAutomatic = true;
	AmmoPerClip = 30;
	MaxAmmo = 90;
	AmmoBoxRounds = 30;
	AmmoBoxPrice = 60;
	TimeBetweenShots = 0.0875f;
	ReloadDuration = 3.05f;
	HitDamage = 32.0f;
	RangeModifier = 0.97f;
	ArmorRatio = 1.4f;
	SpeedModifier = 0.92f;
	Price = 3100;
	// 5.56 mm: 35 units, one wall, 4000 units.
	PenetrationCount = 2;
	PenetrationPower = 88.9f;
	PenetrationDistance = 10160.0f;
	// CS's M4A1PrimaryAttack: the air (0.035 + 0.4 x the accuracy), past 140 units a second (0.035 + 0.07 x), else
	// 0.02 x.
	WeaponSpread = 0.3f;
	WalkingSpread = 0.9f;
	MovingSpread = 3.5f;
	JumpingSpread = 7.0f;
	CrouchingSpreadMod = 0.55f;
	FiringSpreadIncrement = 0.35f;
	RecoilPitch = 0.8f;
	RecoilPitchRandom = 0.25f;
	RecoilYawRandom = 0.35f;
	// CS: the silencer takes 2 s; silenced it hits for 33 but falls off faster and spreads a quarter more.
	bHasSilencer = true;
	SilencerDuration = 2.0f;
	SilencedHitDamage = 33.0f;
	SilencedRangeModifier = 0.95f;
	SilencedSpreadScale = 1.25f;
}
