#include "ShooterCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "ShooterBomb.h"
#include "ShooterCharacterMovement.h"
#include "ShooterGame.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"
#include "Weapons/ShooterProjectile.h"
#include "Weapons/ShooterWeapon.h"

namespace
{

	/** CS's hull at 2.54 cm a unit: 32 units wide, 72 tall (cm). */
	constexpr float ShooterCapsuleRadius = 40.0f;
	constexpr float ShooterCapsuleHalfHeight = 91.5f;

	/** The vertical field of view of CS's 90 degrees at 4:3 (degrees). */
	constexpr float ShooterFieldOfView = 74.0f;

	/** The part of a hit an armored victim's health takes per point of ArmorRatio, and the armor per point of the rest.
	 */
	constexpr float ArmorRatioScale = 0.5f;
	constexpr float ArmorBonus = 0.5f;

	/** How far ahead of the feet a dropped weapon lands, and how far down its floor is looked for (cm). */
	constexpr float DropDistance = 70.0f;
	constexpr float DropFloorSearch = 400.0f;

	/** The slots in the order the best weapon is chosen. */
	constexpr EShooterWeaponSlot SlotsByPreference[] = {
		EShooterWeaponSlot::Primary, EShooterWeaponSlot::Secondary, EShooterWeaponSlot::Grenade};

	/** The armor ratio of what caused the damage: a weapon's or a projectile's; a negative value ignores armor. */
	float GetCauserArmorRatio(const AActor* DamageCauser)
	{
		if (const AShooterWeapon* Weapon = Cast<AShooterWeapon>(DamageCauser))
		{
			return Weapon->ArmorRatio;
		}
		if (const AShooterProjectile* Projectile = Cast<AShooterProjectile>(DamageCauser))
		{
			return Projectile->ArmorRatio;
		}
		return -1.0f;
	}

	/** The headshot multiplier of what caused the damage (1 for anything but a weapon). */
	float GetCauserHeadshotMultiplier(const AActor* DamageCauser)
	{
		const AShooterWeapon* Weapon = Cast<AShooterWeapon>(DamageCauser);
		return Weapon != nullptr ? Weapon->HeadshotMultiplier : 1.0f;
	}

} // namespace

AShooterCharacter::AShooterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UShooterCharacterMovement>(
		  ACharacter::CharacterMovementComponentName))
{
	GetCapsuleComponent()->InitCapsuleSize(ShooterCapsuleRadius, ShooterCapsuleHalfHeight);
	BaseEyeHeight = StandingEyeHeight;
	CrouchedEyeHeight = CrouchingEyeHeight;

	// UE FPS template: the pawn turns with the controller's yaw; the camera takes the pitch.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bOrientRotationToMovement = false;

	// UE FPS template: FirstPersonCameraComponent on the capsule at the eyes, following the control rotation. Leon's
	// capsule stands on the feet, so the eyes are BaseEyeHeight above the component.
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->RelativeLocation = FVector(0.0f, 0.0f, StandingEyeHeight);
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->SetFieldOfView(ShooterFieldOfView);

	// The body the others see: a team mesh standing on the feet (UpdateBody), not in its own player's view (UE
	// ShooterGame: Mesh3P, bOwnerNoSee). No collision: the capsule collides.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->bOwnerNoSee = true;
}

UShooterCharacterMovement* AShooterCharacter::GetShooterCharacterMovement() const
{
	return Cast<UShooterCharacterMovement>(const_cast<UCharacterMovementComponent*>(&GetCharacterMovement()));
}

EShooterTeam AShooterCharacter::GetTeam() const
{
	if (!IsAlive() && DeadTeam != EShooterTeam::None)
	{
		return DeadTeam;
	}
	const AController* OwningController = GetController();
	const AShooterPlayerState* State =
		OwningController != nullptr ? OwningController->GetPlayerState<AShooterPlayerState>() : nullptr;
	return State != nullptr ? State->GetTeam() : EShooterTeam::None;
}

float AShooterCharacter::GetDefaultFieldOfView()
{
	return ShooterFieldOfView;
}

bool AShooterCharacter::IsFirstPerson() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	return IsAlive() && PlayerController != nullptr && PlayerController->IsLocalPlayerController();
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent != nullptr);
	// Look first, then move: a frame moves along the view the mouse just turned.
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AShooterCharacter::AddControllerYawInput);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AShooterCharacter::AddControllerPitchInput);
	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AShooterCharacter::MoveRight);
	PlayerInputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &AShooterCharacter::OnJumpPressed);
	PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Pressed, this, &AShooterCharacter::OnCrouchPressed);
	PlayerInputComponent->BindAction(TEXT("Crouch"), IE_Released, this, &AShooterCharacter::OnCrouchReleased);
	PlayerInputComponent->BindAction(TEXT("Walk"), IE_Pressed, this, &AShooterCharacter::OnWalkPressed);
	PlayerInputComponent->BindAction(TEXT("Walk"), IE_Released, this, &AShooterCharacter::OnWalkReleased);
	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AShooterCharacter::StartWeaponFire);
	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Released, this, &AShooterCharacter::StopWeaponFire);
	PlayerInputComponent->BindAction(TEXT("Targeting"), IE_Pressed, this, &AShooterCharacter::StartSecondaryFire);
	PlayerInputComponent->BindAction(TEXT("Reload"), IE_Pressed, this, &AShooterCharacter::ReloadWeapon);
	PlayerInputComponent->BindAction(TEXT("PrimaryWeapon"), IE_Pressed, this, &AShooterCharacter::OnSelectPrimary);
	PlayerInputComponent->BindAction(TEXT("SecondaryWeapon"), IE_Pressed, this, &AShooterCharacter::OnSelectSecondary);
	PlayerInputComponent->BindAction(TEXT("Grenade"), IE_Pressed, this, &AShooterCharacter::OnSelectGrenade);
	PlayerInputComponent->BindAction(TEXT("DropWeapon"), IE_Pressed, this, &AShooterCharacter::OnDropWeapon);
	PlayerInputComponent->BindAction(TEXT("Use"), IE_Pressed, this, &AShooterCharacter::OnUsePressed);
	PlayerInputComponent->BindAction(TEXT("Use"), IE_Released, this, &AShooterCharacter::OnUseReleased);
}

const AShooterGameState* AShooterCharacter::GetShooterGameState() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetAuthGameMode() != nullptr
		? World->GetAuthGameMode()->GetGameState<AShooterGameState>()
		: nullptr;
}

bool AShooterCharacter::IsFrozen() const
{
	const AShooterGameState* State = GetShooterGameState();
	return State != nullptr && (State->IsFreezeTime() || State->GetRoundState() == EShooterRoundState::MatchEnd);
}

void AShooterCharacter::MoveForward(float Value)
{
	// Frozen, planting or defusing: no walking (CS).
	if (Value != 0.0f && IsAlive() && !IsFrozen() && !bIsPlanting && !IsDefusing())
	{
		// Along the view's yaw, level (the pitch does not slow the walk). ACharacter's one-argument AddMovementInput
		// is Leon's wish setter; the pawn's input vector is APawn's (UE's).
		const FRotator YawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
		APawn::AddMovementInput(YawRotation.Vector(), Value);
	}
}

void AShooterCharacter::MoveRight(float Value)
{
	if (Value != 0.0f && IsAlive() && !IsFrozen() && !bIsPlanting && !IsDefusing())
	{
		const FRotator YawRotation(0.0f, GetControlRotation().Yaw + 90.0f, 0.0f);
		APawn::AddMovementInput(YawRotation.Vector(), Value);
	}
}

void AShooterCharacter::OnJumpPressed()
{
	if (IsAlive() && !IsFrozen())
	{
		Jump();
	}
}

void AShooterCharacter::OnCrouchPressed()
{
	if (IsAlive())
	{
		Crouch();
	}
}

void AShooterCharacter::OnCrouchReleased()
{
	UnCrouch();
}

void AShooterCharacter::OnWalkPressed()
{
	SetWalking(true);
}

void AShooterCharacter::OnWalkReleased()
{
	SetWalking(false);
}

void AShooterCharacter::SetWalking(bool bNewWalking)
{
	if (UShooterCharacterMovement* Move = GetShooterCharacterMovement())
	{
		Move->bIsWalking = bNewWalking;
	}
}

namespace
{

	/** The number keys are the buy menu's while it is open. */
	bool IsBuyMenuOpen(const AController* Controller)
	{
		const AShooterPlayerController* Player = Cast<AShooterPlayerController>(Controller);
		return Player != nullptr && Player->IsBuyMenuOpen();
	}

} // namespace

void AShooterCharacter::OnSelectPrimary()
{
	if (!IsBuyMenuOpen(GetController()))
	{
		SelectSlot(EShooterWeaponSlot::Primary);
	}
}

void AShooterCharacter::OnSelectSecondary()
{
	if (!IsBuyMenuOpen(GetController()))
	{
		SelectSlot(EShooterWeaponSlot::Secondary);
	}
}

void AShooterCharacter::OnSelectGrenade()
{
	if (!IsBuyMenuOpen(GetController()))
	{
		SelectSlot(EShooterWeaponSlot::Grenade);
	}
}

void AShooterCharacter::OnDropWeapon()
{
	// CS drops the rifle or the pistol in hand; grenades stay (plan P18's rule).
	if (CurrentWeapon != nullptr && CurrentWeapon->Slot != EShooterWeaponSlot::Grenade && DropWeapon(CurrentWeapon))
	{
		EquipBestWeapon();
	}
}

void AShooterCharacter::StartWeaponFire()
{
	if (CurrentWeapon != nullptr && IsAlive() && !IsFrozen() && !bIsPlanting && !IsDefusing())
	{
		CurrentWeapon->StartFire();
	}
}

void AShooterCharacter::StopWeaponFire()
{
	if (CurrentWeapon != nullptr)
	{
		CurrentWeapon->StopFire();
	}
}

void AShooterCharacter::StartSecondaryFire()
{
	if (CurrentWeapon != nullptr && IsAlive())
	{
		CurrentWeapon->StartSecondaryFire();
	}
}

void AShooterCharacter::ReloadWeapon()
{
	if (CurrentWeapon != nullptr && IsAlive())
	{
		CurrentWeapon->StartReload();
	}
}

void AShooterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The eyes follow the crouch smoothly (the capsule changes at once; CS lowers the view over a moment).
	FVector& EyeLocation = FirstPersonCameraComponent->RelativeLocation;
	EyeLocation.Z = FMath::FInterpTo(EyeLocation.Z, BaseEyeHeight, DeltaSeconds, EyeHeightInterpSpeed);
	if (bIsPlanting)
	{
		TickPlanting();
	}
}

// The bomb

FName AShooterCharacter::GetBombSiteHere() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return NAME_None;
	}
	const ATriggerVolume* Site =
		AShooterGameMode::FindZone(*World, GetActorLocation(), AShooterGameMode::BombSiteTag, NAME_None);
	return Site != nullptr ? AShooterGameMode::GetZoneName(*Site, AShooterGameMode::BombSiteTag) : NAME_None;
}

bool AShooterCharacter::CanPlant() const
{
	constexpr float StillSpeed = 20.0f;
	// Only while the round is fought (CS: not in the freeze nor after the round's end).
	const AShooterGameState* State = GetShooterGameState();
	const bool bRoundLive = State == nullptr || State->GetRoundState() == EShooterRoundState::Live;
	return IsAlive() && bRoundLive && CarriedBomb != nullptr && !CarriedBomb->IsPendingKillPending() &&
		IsMovingOnGround() && GetCharacterMovement().Velocity.Size2D() <= StillSpeed && GetBombSiteHere() != NAME_None;
}

bool AShooterCharacter::StartUse()
{
	if (!IsAlive() || IsFrozen() || bIsPlanting || IsDefusing())
	{
		return false;
	}
	if (CarriedBomb != nullptr)
	{
		if (!CanPlant())
		{
			return false;
		}
		bIsPlanting = true;
		StopWeaponFire();
		const UWorld* World = GetWorld();
		PlantEndTime = (World != nullptr ? World->GetTimeSeconds() : 0.0f) + CarriedBomb->PlantDuration;
		UE_LOG(LogShooter, Log, TEXT("%s is planting the bomb at %s"), *GetName(), *GetBombSiteHere().ToString());
		return true;
	}
	if (GetTeam() != EShooterTeam::CT)
	{
		return false;
	}
	UWorld* World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return false;
	}
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		AShooterBomb* Bomb = Cast<AShooterBomb>(Actor);
		if (Bomb != nullptr && !Bomb->IsPendingKillPending() && Bomb->StartDefuse(this))
		{
			DefusingBomb = Bomb;
			StopWeaponFire();
			return true;
		}
	}
	return false;
}

void AShooterCharacter::StopUse()
{
	if (bIsPlanting)
	{
		bIsPlanting = false;
		PlantEndTime = 0.0f;
	}
	if (DefusingBomb != nullptr)
	{
		DefusingBomb->StopDefuse(this);
		DefusingBomb = nullptr;
	}
}

void AShooterCharacter::OnUsePressed()
{
	(void)StartUse();
}

void AShooterCharacter::OnUseReleased()
{
	StopUse();
}

void AShooterCharacter::TickPlanting()
{
	if (!CanPlant())
	{
		UE_LOG(LogShooter, Log, TEXT("%s stopped planting"), *GetName());
		StopUse();
		return;
	}
	const UWorld* World = GetWorld();
	if (World != nullptr && World->GetTimeSeconds() >= PlantEndTime)
	{
		AShooterBomb* Bomb = CarriedBomb;
		const FName Site = GetBombSiteHere();
		bIsPlanting = false;
		PlantEndTime = 0.0f;
		// On the floor at the feet, a little ahead (CS puts it down in front of the planter).
		const FVector Ahead = FRotator(0.0f, GetActorRotation().Yaw, 0.0f).Vector() * 30.0f;
		Bomb->Plant(GetActorLocation() + Ahead, Site, this);
	}
}

void AShooterCharacter::ResetForNewRound(const FVector& Feet, float Yaw)
{
	StopUse();
	StopWeaponFire();
	UnCrouch();
	SetWalking(false);
	Health = MaxHealth;
	GetCharacterMovement().Velocity = FVector::ZeroVector;
	Reset(Feet, FRotator(0.0f, Yaw, 0.0f));
	if (AController* OwningController = GetController())
	{
		OwningController->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
	}
	if (CurrentWeapon != nullptr)
	{
		// Put away and drawn again: the zoom, a reload and the recoil end.
		AShooterWeapon* Weapon = CurrentWeapon;
		Weapon->OnUnEquip();
		Weapon->OnEquip();
	}
}

void AShooterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// UE ShooterGame: the health is set here, so a pawn is alive from its spawn (BeginPlay may come later in the
	// frame).
	ResetHealth();
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();
	SpawnDefaultInventory();
}

void AShooterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyInventory();
	Super::EndPlay(EndPlayReason);
}

void AShooterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	UpdateBody();
	// The meshes that show by owner read the owner chain when their proxies are made: the controller is in it now.
	BodyMesh->MarkRenderStateDirty();
	for (AShooterWeapon* Weapon : Inventory)
	{
		Weapon->OnEnterInventory(this);
	}
}

void AShooterCharacter::UpdateBody()
{
	const EShooterTeam Team = GetTeam();
	const FSoftObjectPath& MeshName = Team == EShooterTeam::T ? TBodyMeshName : CTBodyMeshName;
	if (Team == EShooterTeam::None || MeshName.IsNull() ||
		!FPackageName::DoesPackageExist(MeshName.GetLongPackageName()))
	{
		return;
	}
	if (UStaticMesh* TeamMesh = Cast<UStaticMesh>(MeshName.TryLoad()))
	{
		(void)BodyMesh->SetStaticMesh(TeamMesh);
		BodyMesh->SetVisibility(true);
	}
}

// Health, armor and death

void AShooterCharacter::ResetHealth()
{
	Health = MaxHealth;
	Armor = 0.0f;
	bHasHelmet = false;
	DeadTeam = EShooterTeam::None;
}

void AShooterCharacter::SetArmor(float NewArmor, bool bNewHasHelmet)
{
	Armor = FMath::Clamp(NewArmor, 0.0f, MaxArmor);
	bHasHelmet = bNewHasHelmet;
}

EShooterHitGroup AShooterCharacter::GetHitGroup(const FVector& Location) const
{
	// The actor stands on its feet: the capsule's top is two half heights up.
	const float Top = GetActorLocation().Z + (2.0f * GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	return Location.Z >= Top - HeadHeight ? EShooterHitGroup::Head : EShooterHitGroup::Body;
}

void AShooterCharacter::ComputeArmorDamage(float Damage, float ArmorRatio, bool bHeadshot, bool bHelmet, float Armor,
	float& OutHealthDamage, float& OutArmorDamage)
{
	OutHealthDamage = Damage;
	OutArmorDamage = 0.0f;
	// CS: armor needs points, a weapon that respects it, and on the head a helmet.
	if (Armor <= 0.0f || ArmorRatio < 0.0f || (bHeadshot && !bHelmet))
	{
		return;
	}
	OutHealthDamage = Damage * FMath::Min(1.0f, ArmorRatio * ArmorRatioScale);
	OutArmorDamage = (Damage - OutHealthDamage) * ArmorBonus;
	if (OutArmorDamage > Armor)
	{
		// The armor gives out: what it could not stop goes to health.
		OutArmorDamage = Armor;
		OutHealthDamage = Damage - (Armor / ArmorBonus);
	}
}

float AShooterCharacter::TakeDamage(
	float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!IsAlive() || bGodMode)
	{
		return 0.0f;
	}
	UWorld* World = GetWorld();
	AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	if (GameMode != nullptr && !GameMode->CanDealDamage(EventInstigator, GetController()))
	{
		return 0.0f;
	}
	// UE ShooterGame: the actor's damage first (bCanBeDamaged, the radial falloff, the delegates).
	float Amount = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	if (Amount <= 0.0f)
	{
		return 0.0f;
	}

	bool bHeadshot = false;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		bHeadshot =
			PointEvent.HitInfo.bBlockingHit && GetHitGroup(PointEvent.HitInfo.ImpactPoint) == EShooterHitGroup::Head;
		if (bHeadshot)
		{
			Amount *= GetCauserHeadshotMultiplier(DamageCauser);
		}
	}

	float HealthDamage = 0.0f;
	float ArmorDamage = 0.0f;
	ComputeArmorDamage(
		Amount, GetCauserArmorRatio(DamageCauser), bHeadshot, bHasHelmet, Armor, HealthDamage, ArmorDamage);
	Armor = FMath::Max(0.0f, Armor - ArmorDamage);
	const float Taken = FMath::Min(Health, HealthDamage);
	Health -= Taken;
	if (Health <= 0.0f)
	{
		Health = 0.0f;
		Die(EventInstigator != nullptr ? EventInstigator : GetController(), DamageCauser, bHeadshot);
	}
	return Taken;
}

void AShooterCharacter::Suicide()
{
	if (IsAlive())
	{
		Health = 0.0f;
		Die(GetController(), nullptr, false);
	}
}

void AShooterCharacter::Die(AController* Killer, AActor* DamageCauser, bool bHeadshot)
{
	AController* Victim = GetController();
	DeadTeam = EShooterTeam::None;
	if (const AShooterPlayerState* State = Victim != nullptr ? Victim->GetPlayerState<AShooterPlayerState>() : nullptr)
	{
		DeadTeam = State->GetTeam();
	}
	UWorld* World = GetWorld();
	if (AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr)
	{
		GameMode->Killed(Killer, Victim, this, DamageCauser, bHeadshot);
	}

	// CS: the bomb and the best of the rifle and the pistol fall; the rest, and the kit, are lost.
	StopUse();
	StopWeaponFire();
	if (CarriedBomb != nullptr)
	{
		CarriedBomb->Drop(GetActorLocation());
	}
	bHasDefuseKit = false;
	AShooterWeapon* Dropped = GetWeaponInSlot(EShooterWeaponSlot::Primary);
	if (Dropped == nullptr)
	{
		Dropped = GetWeaponInSlot(EShooterWeaponSlot::Secondary);
	}
	if (Dropped != nullptr)
	{
		(void)DropWeapon(Dropped);
	}
	DestroyInventory();

	// The corpse: no collision (shots go through), no movement, the body lying on its back.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement().Velocity = FVector::ZeroVector;
	GetCharacterMovement().SetComponentTickEnabled(false);
	BodyMesh->SetRelativeLocationAndRotation(
		FVector(0.0f, 0.0f, ShooterCapsuleRadius * 0.5f), FRotator(90.0f, 0.0f, 0.0f));

	if (APlayerController* PlayerController = Cast<APlayerController>(Victim))
	{
		PlayerController->ChangeState(NAME_Spectating);
	}
	else if (Victim != nullptr)
	{
		Victim->UnPossess();
	}
	// Seen by everyone now, its own player included (the owner chain changed).
	BodyMesh->MarkRenderStateDirty();
	if (CorpseLifeSpan > 0.0f)
	{
		SetLifeSpan(CorpseLifeSpan);
	}
}

// Inventory

void AShooterCharacter::SpawnDefaultInventory()
{
	for (const FString& WeaponName : DefaultWeapons)
	{
		UClass* WeaponClass = AShooterWeapon::FindWeaponClass(WeaponName);
		if (WeaponClass == nullptr)
		{
			UE_LOG(LogShooter, Warning, TEXT("%s: DefaultWeapons names '%s', which is no weapon"), *GetName(),
				*WeaponName);
			continue;
		}
		(void)GiveWeapon(WeaponClass);
	}
	EquipBestWeapon();
}

AShooterWeapon* AShooterCharacter::GiveWeapon(UClass* WeaponClass)
{
	UWorld* World = GetWorld();
	if (World == nullptr || WeaponClass == nullptr || !WeaponClass->IsChildOf(AShooterWeapon::StaticClass()) ||
		WeaponClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = this;
	SpawnInfo.ObjectFlags |= RF_Transient;
	AShooterWeapon* Weapon =
		World->SpawnActor<AShooterWeapon>(WeaponClass, GetActorLocation(), FRotator::ZeroRotator, SpawnInfo);
	if (Weapon != nullptr)
	{
		AddWeapon(Weapon);
	}
	return Weapon;
}

void AShooterCharacter::AddWeapon(AShooterWeapon* Weapon)
{
	if (Weapon == nullptr || Inventory.Contains(Weapon))
	{
		return;
	}
	if (AShooterWeapon* Old = GetWeaponInSlot(Weapon->Slot))
	{
		(void)DropWeapon(Old);
	}
	Inventory.Add(Weapon);
	Weapon->OnEnterInventory(this);
	if (CurrentWeapon == nullptr)
	{
		EquipWeapon(Weapon);
	}
}

void AShooterCharacter::RemoveWeapon(AShooterWeapon* Weapon)
{
	if (Weapon == nullptr || !Inventory.Contains(Weapon))
	{
		return;
	}
	const bool bWasCurrent = Weapon == CurrentWeapon;
	if (bWasCurrent)
	{
		CurrentWeapon = nullptr;
	}
	Inventory.Remove(Weapon);
	Weapon->OnLeaveInventory();
	if (bWasCurrent && IsAlive())
	{
		EquipBestWeapon();
	}
}

void AShooterCharacter::EquipWeapon(AShooterWeapon* Weapon)
{
	if (Weapon == nullptr || Weapon == CurrentWeapon || !Inventory.Contains(Weapon))
	{
		return;
	}
	if (CurrentWeapon != nullptr)
	{
		CurrentWeapon->OnUnEquip();
	}
	CurrentWeapon = Weapon;
	CurrentWeapon->OnEquip();
}

void AShooterCharacter::SelectSlot(EShooterWeaponSlot Slot)
{
	if (IsAlive())
	{
		EquipWeapon(GetWeaponInSlot(Slot));
	}
}

void AShooterCharacter::EquipBestWeapon()
{
	for (const EShooterWeaponSlot Slot : SlotsByPreference)
	{
		if (AShooterWeapon* Weapon = GetWeaponInSlot(Slot))
		{
			EquipWeapon(Weapon);
			return;
		}
	}
}

AShooterWeapon* AShooterCharacter::GetWeaponInSlot(EShooterWeaponSlot Slot) const
{
	for (AShooterWeapon* Weapon : Inventory)
	{
		if (Weapon != nullptr && Weapon->Slot == Slot)
		{
			return Weapon;
		}
	}
	return nullptr;
}

bool AShooterCharacter::DropWeapon(AShooterWeapon* Weapon)
{
	UWorld* World = GetWorld();
	if (Weapon == nullptr || World == nullptr || !Inventory.Contains(Weapon))
	{
		return false;
	}
	if (Weapon == CurrentWeapon)
	{
		CurrentWeapon = nullptr;
	}
	Inventory.Remove(Weapon);
	Weapon->OnLeaveInventory();

	// Ahead of the feet, short of a wall, on the floor below (the Visibility channel ignores the pawns).
	const FVector Feet = GetActorLocation();
	const FVector Knee = Feet + FVector(0.0f, 0.0f, ShooterCapsuleRadius);
	const FVector Ahead = Knee + (FRotator(0.0f, GetActorRotation().Yaw, 0.0f).Vector() * DropDistance);
	FCollisionQueryParams Params(FName(TEXT("DropWeapon")), false, this);
	Params.AddIgnoredActor(Weapon);
	FHitResult Hit;
	FVector Location = Ahead;
	if (UGameplayStatics::LineTraceSingleByChannel(*World, Hit, Knee, Ahead, ECC_Visibility, Params))
	{
		Location = Hit.Location - ((Ahead - Knee).GetSafeNormal() * ShooterCapsuleRadius * 0.5f);
	}
	FHitResult Floor;
	const bool bFloor = UGameplayStatics::LineTraceSingleByChannel(
		*World, Floor, Location, Location - FVector(0.0f, 0.0f, DropFloorSearch), ECC_Visibility, Params);
	Weapon->OnDropped(bFloor ? Floor.Location : Feet, GetActorRotation().Yaw);
	return true;
}

void AShooterCharacter::DestroyInventory()
{
	CurrentWeapon = nullptr;
	const TArray<AShooterWeapon*> Weapons = Inventory;
	Inventory.Reset();
	for (AShooterWeapon* Weapon : Weapons)
	{
		if (Weapon != nullptr)
		{
			Weapon->OnLeaveInventory();
			(void)Weapon->Destroy();
		}
	}
}
