#include "ShooterCharacter.h"

#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/CharacterAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
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
#include "Sound/SoundWave.h"
#include "TimerManager.h"
#include "Weapons/ShooterProjectile.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterWeapon_Projectile.h"

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

	/** How far below a pawn killed in the air its floor is looked for (cm): the corpse and the bomb land on it. */
	constexpr float DeathFloorSearch = 5000.0f;

	/** CS: a step is heard above 150 units a second (cm/s); walking and crouching are slower. */
	constexpr float FootstepMinSpeed = 381.0f;

	/** The floor's line: from a little above the feet to 50 cm under them (cm). */
	constexpr float FloorTraceUp = 10.0f;
	constexpr float FloorTraceDown = 50.0f;

	/** The slots in the order the best weapon is chosen (the knife last: it never runs out). */
	constexpr EShooterWeaponSlot SlotsByPreference[] = {EShooterWeaponSlot::Primary, EShooterWeaponSlot::Secondary,
		EShooterWeaponSlot::Grenade, EShooterWeaponSlot::Knife};

	/**
	 * The armor ratio of what caused the damage: a weapon's, a projectile's (the HE grenade) or the bomb's (CS 1.6's
	 * blasts are armored like the rest); a negative value ignores armor (the world: falls, suicides).
	 */
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
		if (const AShooterBomb* Bomb = Cast<AShooterBomb>(DamageCauser))
		{
			return Bomb->ArmorRatio;
		}
		return -1.0f;
	}

	/** The headshot multiplier of what caused the damage (1 for anything but a weapon). */
	float GetCauserHeadshotMultiplier(const AActor* DamageCauser)
	{
		const AShooterWeapon* Weapon = Cast<AShooterWeapon>(DamageCauser);
		return Weapon != nullptr ? Weapon->HeadshotMultiplier : 1.0f;
	}

	/**
	 * Where damage came from, for the damage direction indicator: a shot from its shooter (the weapon is held), a blast
	 * (a grenade, the bomb) from itself; false for the world's damage or one's own shot.
	 */
	bool GetDamageSourceLocation(
		const AActor& Victim, const AController* Instigator, const AActor* DamageCauser, FVector& OutLocation)
	{
		const AActor* Source = DamageCauser;
		if (DamageCauser == nullptr || DamageCauser->IsA<AShooterWeapon>())
		{
			Source = Instigator != nullptr ? Instigator->GetPawn() : nullptr;
		}
		if (Source == nullptr || Source == &Victim)
		{
			return false;
		}
		OutLocation = Source->GetActorLocation();
		return true;
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

	// The body the others see (UE ShooterGame: Mesh3P): ACharacter's skeletal mesh, standing on the feet, the team's
	// (UpdateBody), not in its own player's view, its pose evaluated only when drawn and less often far away, with a
	// blob shadow on the floor under it (N15).
	USkeletalMeshComponent& Body = GetMesh();
	Body.bOwnerNoSee = true;
	Body.bCastBlobShadow = true;
	Body.VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
	Body.bEnableUpdateRateOptimizations = true;

	// UE ShooterGame: Mesh1P, the arms on the camera, drawn in the view model pass of their player's view only.
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PawnMesh1P"));
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bOnlyOwnerSee = true;
	Mesh1P->bRenderAsViewModel = true;
	Mesh1P->CastShadow = false;
	// Posed only when drawn: a bot's arms play their montages (the draw and reload times) but are never evaluated.
	Mesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
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
	PlayerInputComponent->BindAxis(TEXT("TurnRate"), this, &AShooterCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis(TEXT("LookUpRate"), this, &AShooterCharacter::LookUpAtRate);
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
	PlayerInputComponent->BindAction(TEXT("Knife"), IE_Pressed, this, &AShooterCharacter::OnSelectKnife);
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

void AShooterCharacter::TurnAtRate(float Rate)
{
	const UWorld* World = GetWorld();
	if (Rate != 0.0f && World != nullptr)
	{
		AddControllerYawInput(Rate * BaseTurnRate * GetAimSensitivity() * World->GetDeltaSeconds());
	}
}

void AShooterCharacter::LookUpAtRate(float Rate)
{
	const UWorld* World = GetWorld();
	if (Rate != 0.0f && World != nullptr)
	{
		AddControllerPitchInput(Rate * BaseLookUpRate * GetAimSensitivity() * World->GetDeltaSeconds());
	}
}

float AShooterCharacter::GetAimSensitivity() const
{
	// The player's options scale the stick (N24); a bot's controller has none.
	const AShooterPlayerController* PlayerController = Cast<AShooterPlayerController>(GetController());
	return PlayerController != nullptr ? PlayerController->GetAimSensitivity() : 1.0f;
}

void AShooterCharacter::PerformMovement(FPhysScene& PhysScene, float DeltaTime, FDebugDraw* DebugDraw)
{
	if (UShooterCharacterMovement* Move = GetShooterCharacterMovement())
	{
		Move->UpdateMovementTimers(DeltaTime);
	}
	Super::PerformMovement(PhysScene, DeltaTime, DebugDraw);
}

void AShooterCharacter::Jump()
{
	UShooterCharacterMovement* Move = GetShooterCharacterMovement();
	if (Move != nullptr && Move->IsOnLadder())
	{
		Move->JumpOffLadder();
		return;
	}
	Super::Jump();
}

void AShooterCharacter::OnJumped()
{
	Super::OnJumped();
	if (UShooterCharacterMovement* Move = GetShooterCharacterMovement())
	{
		Move->StartJumpStamina();
	}
}

void AShooterCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	const UShooterCharacterMovement* Move = GetShooterCharacterMovement();
	const float LandingSpeed = -GetVelocityZ();
	const float FallDamage = Move != nullptr ? Move->GetFallDamage(LandingSpeed) : 0.0f;
	if (FallDamage > 0.0f && IsAlive())
	{
		// CS's DMG_FALL is the world's: no instigator and no causer (no armor, no tagging, the kill feed's world).
		UE_LOG(LogShooter, Log, TEXT("%s landed at %.0f cm/s: %.0f fall damage"), *GetName(),
			static_cast<double>(LandingSpeed), static_cast<double>(FallDamage));
		(void)TakeDamage(FallDamage, FDamageEvent(), nullptr, nullptr);
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

void AShooterCharacter::OnSelectKnife()
{
	if (!IsBuyMenuOpen(GetController()))
	{
		SelectSlot(EShooterWeaponSlot::Knife);
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
	// CS drops the rifle or the pistol in hand; the knife and the grenades stay (plan P18's rule).
	if (CurrentWeapon != nullptr && CurrentWeapon->CanBeDropped() && DropWeapon(CurrentWeapon))
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
	UpdateBodyAnimation();
	Super::Tick(DeltaSeconds);
	// The arms tick with the pawn, as ACharacter ticks its mesh (a skeletal mesh component has no tick function of its
	// own): their montages advance and their pose is evaluated when drawn.
	Mesh1P->TickComponent(DeltaSeconds);
	// The eyes follow the crouch smoothly (the capsule changes at once; CS lowers the view over a moment).
	FVector& EyeLocation = FirstPersonCameraComponent->RelativeLocation;
	EyeLocation.Z = FMath::FInterpTo(EyeLocation.Z, BaseEyeHeight, DeltaSeconds, EyeHeightInterpSpeed);
	if (bIsPlanting)
	{
		TickPlanting();
	}
	TickLadderSteps(DeltaSeconds);
}

// The bomb

FName AShooterCharacter::GetBombSiteHere() const
{
	const AShooterGameMode* GameMode = GetShooterGameMode();
	const ATriggerVolume* Site = GameMode != nullptr
		? GameMode->FindZone(GetActorLocation(), AShooterGameMode::BombSiteTag, NAME_None)
		: nullptr;
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
		(void)PlayPawnMontages(PlantAnim);
		GetWorldTimerManager().SetTimer(
			TimerHandle_Plant, this, &AShooterCharacter::OnPlantTimer, CarriedBomb->PlantDuration);
		UE_LOG(LogShooter, Log, TEXT("%s is planting the bomb at %s"), *GetName(), *GetBombSiteHere().ToString());
		return true;
	}
	if (GetTeam() != EShooterTeam::CT)
	{
		return false;
	}
	const AShooterGameMode* GameMode = GetShooterGameMode();
	if (GameMode == nullptr)
	{
		return false;
	}
	for (AShooterBomb* Bomb : GameMode->GetBombs())
	{
		if (!Bomb->IsPendingKillPending() && Bomb->StartDefuse(this))
		{
			DefusingBomb = Bomb;
			StopWeaponFire();
			(void)PlayPawnMontages(DefuseAnim);
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
		StopPawnMontages(PlantAnim);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TimerHandle_Plant);
		}
	}
	if (DefusingBomb != nullptr)
	{
		DefusingBomb->StopDefuse(this);
		DefusingBomb = nullptr;
		StopPawnMontages(DefuseAnim);
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
	}
}

void AShooterCharacter::OnPlantTimer()
{
	if (!bIsPlanting)
	{
		return;
	}
	if (!CanPlant())
	{
		UE_LOG(LogShooter, Log, TEXT("%s stopped planting"), *GetName());
		StopUse();
		return;
	}
	AShooterBomb* Bomb = CarriedBomb;
	const FName Site = GetBombSiteHere();
	bIsPlanting = false;
	// On the floor at the feet, a little ahead (CS puts it down in front of the planter).
	const FVector Ahead = FRotator(0.0f, GetActorRotation().Yaw, 0.0f).Vector() * 30.0f;
	Bomb->Plant(GetActorLocation() + Ahead, Site, this);
}

float AShooterCharacter::GetPlantEndTime() const
{
	const UWorld* World = GetWorld();
	const float Remaining = World != nullptr ? World->GetTimerManager().GetTimerRemaining(TimerHandle_Plant) : -1.0f;
	return Remaining >= 0.0f ? World->GetTimeSeconds() + Remaining : 0.0f;
}

void AShooterCharacter::ResetForNewRound(const FVector& Feet, float Yaw)
{
	StopUse();
	StopWeaponFire();
	UnCrouch();
	SetWalking(false);
	Health = MaxHealth;
	ClearFlash();
	GetCharacterMovement().Velocity = FVector::ZeroVector;
	if (UShooterCharacterMovement* Move = GetShooterCharacterMovement())
	{
		Move->ResetMovementModifiers();
	}
	Reset(Feet, FRotator(0.0f, Yaw, 0.0f));
	if (AController* OwningController = GetController())
	{
		OwningController->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
	}
	// The level view stays level: the last round's recoil is not recovered into it.
	for (AShooterWeapon* Weapon : Inventory)
	{
		if (Weapon != nullptr)
		{
			Weapon->ResetAim();
		}
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
	// The animation assets the config names; the meshes come with the team (UpdateBody).
	LocomotionBlendSpace = LoadShooterAsset<UBlendSpaceBase>(LocomotionBlendSpaceName);
	CrouchBlendSpace = LoadShooterAsset<UBlendSpaceBase>(CrouchBlendSpaceName);
	AimOffset = LoadShooterAsset<UAimOffsetBlendSpace1D>(AimOffsetName);
	JumpClips.JumpStart = LoadShooterAsset<UAnimSequence>(JumpStartAnimName);
	JumpClips.FallLoop = LoadShooterAsset<UAnimSequence>(JumpLoopAnimName);
	JumpClips.Land = LoadShooterAsset<UAnimSequence>(JumpLandAnimName);
	DeathAnimBack = LoadShooterAsset<UAnimMontage>(DeathAnimBackName);
	DeathAnimFront = LoadShooterAsset<UAnimMontage>(DeathAnimFrontName);
	PlantAnim = {LoadShooterAsset<UAnimMontage>(PlantAnim1PName), LoadShooterAsset<UAnimMontage>(PlantAnim3PName)};
	DefuseAnim = {LoadShooterAsset<UAnimMontage>(DefuseAnim1PName), LoadShooterAsset<UAnimMontage>(DefuseAnim3PName)};
	FootstepSoundSet.Load(FootstepSounds);
	LadderStepSounds.Reset();
	for (const FSoftObjectPath& Path : LadderStepSoundNames)
	{
		if (USoundWave* Sound = LoadShooterSound(Path))
		{
			LadderStepSounds.Add(Sound);
		}
	}
}

bool AShooterCharacter::HasSkeletalBody() const
{
	return GetMesh().HasValidMesh();
}

bool AShooterCharacter::HasArms() const
{
	return Mesh1P->HasValidMesh();
}

void AShooterCharacter::SetSkeletalBody(USkeletalMesh* InMesh)
{
	USkeletalMeshComponent& Body = GetMesh();
	Body.SetSkeletalMesh(InMesh);
	if (InMesh != nullptr)
	{
		// The character's anim graph, bound to this pawn's notifies.
		UCharacterAnimInstance& Anim = Body.SetAnimInstance<UCharacterAnimInstance>();
		Anim.SetBlendSpace(LocomotionBlendSpace);
		Anim.SetCrouchBlendSpace(CrouchBlendSpace);
		Anim.SetJumpClips(JumpClips);
		Anim.SetLocomotionBlendInterpSpeed(10.0f);
		Anim.SetUpperBodyBranchBone(UpperBodyBranchBone);
		Anim.SetAimOffset(AimOffset);
		Anim.OnAnimNotify.AddUObject(this, &AShooterCharacter::OnBodyAnimNotify);
	}
	ReattachWeapon();
}

void AShooterCharacter::SetArmsMesh(USkeletalMesh* InMesh)
{
	if (InMesh != nullptr && Mesh1P->GetSkeletalMesh() == InMesh)
	{
		return;
	}
	Mesh1P->SetSkeletalMesh(InMesh);
	if (InMesh != nullptr)
	{
		// The arms play the weapons' montages over the drawn weapon's idle.
		UAnimInstance& Anim = Mesh1P->SetAnimInstance<UAnimInstance>();
		Anim.OnAnimNotify.AddUObject(this, &AShooterCharacter::OnArmsAnimNotify);
		SetArmsIdle(CurrentWeapon != nullptr ? CurrentWeapon->GetArmsIdle() : nullptr);
	}
	ReattachWeapon();
}

void AShooterCharacter::SetArmsIdle(const UBlendSpaceBase* Idle)
{
	if (HasArms())
	{
		Mesh1P->GetAnimInstance().SetBlendSpace(Idle);
	}
}

void AShooterCharacter::ReattachWeapon()
{
	if (CurrentWeapon != nullptr)
	{
		CurrentWeapon->AttachMeshToPawn();
	}
}

USceneComponent* AShooterCharacter::GetWeaponAttachParent1P(FName& OutSocketName) const
{
	if (HasArms() && Mesh1P->DoesSocketExist(WeaponSocketName))
	{
		OutSocketName = WeaponSocketName;
		return Mesh1P;
	}
	OutSocketName = NAME_None;
	return FirstPersonCameraComponent;
}

USceneComponent* AShooterCharacter::GetWeaponAttachParent3P(FName& OutSocketName) const
{
	if (HasSkeletalBody() && GetMesh().DoesSocketExist(WeaponSocketName))
	{
		OutSocketName = WeaponSocketName;
		return const_cast<USkeletalMeshComponent*>(&GetMesh());
	}
	OutSocketName = NAME_None;
	return GetCapsuleComponent();
}

float AShooterCharacter::PlayPawnMontages(const FShooterWeaponAnim& Animation, float PlayRate)
{
	float Duration = 0.0f;
	if (Animation.Pawn1P != nullptr && HasArms())
	{
		Duration = FMath::Max(Duration, Mesh1P->GetAnimInstance().Montage_Play(Animation.Pawn1P, PlayRate) / PlayRate);
	}
	if (Animation.Pawn3P != nullptr && HasSkeletalBody())
	{
		Duration = FMath::Max(Duration, PlayAnimMontage(Animation.Pawn3P, PlayRate));
	}
	return Duration;
}

void AShooterCharacter::StopPawnMontages(const FShooterWeaponAnim& Animation)
{
	if (Animation.Pawn1P != nullptr && HasArms() && Mesh1P->GetAnimInstance().Montage_IsPlaying(Animation.Pawn1P))
	{
		Mesh1P->GetAnimInstance().Montage_Stop(Animation.Pawn1P->BlendOutTime, Animation.Pawn1P);
	}
	if (Animation.Pawn3P != nullptr && HasSkeletalBody())
	{
		StopAnimMontage(Animation.Pawn3P);
	}
}

void AShooterCharacter::UpdateBodyAnimation()
{
	if (!HasSkeletalBody())
	{
		return;
	}
	UAnimInstance& Anim = GetMesh().GetAnimInstance();
	if (UCharacterAnimInstance* CharacterAnim = Cast<UCharacterAnimInstance>(&Anim))
	{
		CharacterAnim->SetCrouched(bIsCrouched);
	}
	// The drawn weapon's stance (its aim offset), none while the whole body is busy: planting, defusing, dead.
	const UAimOffsetBlendSpace1D* Stance = CurrentWeapon != nullptr && CurrentWeapon->GetAimOffset() != nullptr
		? CurrentWeapon->GetAimOffset()
		: AimOffset;
	Anim.SetAimOffset(IsAlive() && !bIsPlanting && !IsDefusing() ? Stance : nullptr);
	// UE's CalculateDirection: the velocity's angle from the pawn's forward, degrees.
	const FVector Velocity = GetCharacterMovement().Velocity;
	const float Speed = Velocity.Size2D();
	float Direction = 0.0f;
	if (Speed > 1.0f)
	{
		const FVector Forward = GetActorForwardVector();
		const FVector Right = GetActorRightVector();
		Direction = FMath::RadiansToDegrees(FMath::Atan2(Velocity | Right, Velocity | Forward));
	}
	Anim.SetBlendSpaceInput(FVector(Speed, Direction, 0.0f));
	float Pitch = GetViewRotation().Pitch;
	Pitch = Pitch > 180.0f ? Pitch - 360.0f : Pitch;
	Anim.SetAimOffsetPitch(IsAlive() ? Pitch : 0.0f);
}

void AShooterCharacter::OnBodyAnimNotify(FName NotifyName, const UAnimSequenceBase* Animation)
{
	(void)Animation;
	static const FName FootstepLeft(TEXT("Footstep_L"));
	static const FName FootstepRight(TEXT("Footstep_R"));
	if (NotifyName == FootstepLeft || NotifyName == FootstepRight)
	{
		PlayFootstep(NotifyName == FootstepLeft);
		return;
	}
	// The weapon's notifies come from the view its player sees: the arms in first person, the body otherwise.
	if (CurrentWeapon != nullptr && !(IsFirstPerson() && HasArms()))
	{
		CurrentWeapon->OnAnimNotify(NotifyName);
	}
}

EPhysicalSurface AShooterCharacter::GetFloorSurface() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return SHOOTER_SURFACE_Default;
	}
	FCollisionQueryParams Params(FName(TEXT("FootstepTrace")), false, this);
	Params.bReturnPhysicalMaterial = true;
	const FVector Feet = GetActorLocation();
	FHitResult Hit;
	if (!World->GetPhysicsScene().LineTraceSingleByChannel(Hit, Feet + FVector(0.0f, 0.0f, FloorTraceUp),
			Feet - FVector(0.0f, 0.0f, FloorTraceDown), ECC_Visibility, Params))
	{
		return SHOOTER_SURFACE_Default;
	}
	return UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
}

void AShooterCharacter::PlayFootstep(bool bLeftFoot)
{
	// CS: only a step on the floor faster than FootstepMinSpeed is heard.
	if (!IsAlive() || !IsMovingOnGround() || GetCharacterMovement().Velocity.Size2D() <= FootstepMinSpeed)
	{
		return;
	}
	// The floor's step, the foot's variant (CS: the left foot's sounds, then the right's).
	LastFootstepSurface = GetFloorSurface();
	LastFootstepSound = FootstepSoundSet.Get(LastFootstepSurface.GetValue(), bLeftFoot ? 0 : 1);
	if (LastFootstepSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LastFootstepSound, GetActorLocation());
	}
	// From the capsule's middle (the feet are on the floor, where a line of sight to them would end in it).
	MakeNoise(FootstepNoiseLoudness, this,
		GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
}

void AShooterCharacter::TickLadderSteps(float DeltaSeconds)
{
	const UShooterCharacterMovement* Move = GetShooterCharacterMovement();
	if (!IsAlive() || Move == nullptr || !Move->IsOnLadder() || Move->Velocity.Size() <= FootstepMinSpeed)
	{
		// The first step of a climb comes at once (CS).
		LadderStepTime = LadderStepInterval;
		return;
	}
	LadderStepTime += DeltaSeconds;
	if (LadderStepTime < LadderStepInterval)
	{
		return;
	}
	LadderStepTime = 0.0f;
	LastFootstepSound =
		LadderStepSounds.Num() > 0 ? LadderStepSounds[NumLadderSteps % LadderStepSounds.Num()] : nullptr;
	++NumLadderSteps;
	if (LastFootstepSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LastFootstepSound, GetActorLocation());
	}
	MakeNoise(FootstepNoiseLoudness, this,
		GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
}

void AShooterCharacter::OnArmsAnimNotify(FName NotifyName, const UAnimSequenceBase* Animation)
{
	(void)Animation;
	if (CurrentWeapon != nullptr && IsFirstPerson())
	{
		CurrentWeapon->OnAnimNotify(NotifyName);
	}
}

AShooterGameMode* AShooterCharacter::GetShooterGameMode() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (AShooterGameMode* GameMode = GetShooterGameMode())
	{
		GameMode->RegisterPawn(this);
	}
	SpawnDefaultInventory();
}

void AShooterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AShooterGameMode* GameMode = GetShooterGameMode())
	{
		GameMode->UnregisterPawn(this);
	}
	DestroyInventory();
	Super::EndPlay(EndPlayReason);
}

void AShooterCharacter::UnPossessed()
{
	Super::UnPossessed();
	if (AShooterGameMode* GameMode = GetShooterGameMode())
	{
		GameMode->NotifyPawnsChanged();
	}
}

void AShooterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (AShooterGameMode* GameMode = GetShooterGameMode())
	{
		GameMode->NotifyPawnsChanged();
	}
	UpdateBody();
	// The meshes that show by owner read the owner chain when their proxies are made: the controller is in it now.
	GetMesh().MarkRenderStateDirty();
	Mesh1P->MarkRenderStateDirty();
	for (AShooterWeapon* Weapon : Inventory)
	{
		Weapon->OnEnterInventory(this);
	}
	// The team is known now: its pistol.
	SpawnTeamInventory();
}

void AShooterCharacter::UpdateBody()
{
	const EShooterTeam Team = GetTeam();
	if (Team == EShooterTeam::None)
	{
		return;
	}
	const bool bTerrorist = Team == EShooterTeam::T;
	if (USkeletalMesh* Body = LoadShooterAsset<USkeletalMesh>(bTerrorist ? TBodyMeshName : CTBodyMeshName))
	{
		SetSkeletalBody(Body);
	}
	if (USkeletalMesh* Arms = LoadShooterAsset<USkeletalMesh>(bTerrorist ? TArmsMeshName : CTArmsMeshName))
	{
		SetArmsMesh(Arms);
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
	// The actor stands on its feet: the capsule's top is two half heights up (crouched, the crouched capsule's).
	const FVector Feet = GetActorLocation();
	const float Height = 2.0f * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Up = Location.Z - Feet.Z;
	if (Up >= Height - HeadHeight)
	{
		return EShooterHitGroup::Head;
	}
	const float BodyHeight = FMath::Max(1.0f, Height - HeadHeight);
	const float Side = (Location - Feet) | GetActorRightVector();
	if (Up < BodyHeight * LegsFraction)
	{
		return Side >= 0.0f ? EShooterHitGroup::RightLeg : EShooterHitGroup::LeftLeg;
	}
	if (Up < BodyHeight * StomachFraction)
	{
		return EShooterHitGroup::Stomach;
	}
	if (FMath::Abs(Side) > ArmFraction * GetCapsuleComponent()->GetScaledCapsuleRadius())
	{
		return Side >= 0.0f ? EShooterHitGroup::RightArm : EShooterHitGroup::LeftArm;
	}
	return EShooterHitGroup::Chest;
}

float AShooterCharacter::GetHitGroupMultiplier(EShooterHitGroup HitGroup, float HeadMultiplier) const
{
	switch (HitGroup)
	{
		case EShooterHitGroup::Head:
			return HeadMultiplier;
		case EShooterHitGroup::Stomach:
			return StomachDamageMultiplier;
		case EShooterHitGroup::LeftLeg:
		case EShooterHitGroup::RightLeg:
			return LegDamageMultiplier;
		case EShooterHitGroup::Generic:
		case EShooterHitGroup::Chest:
		case EShooterHitGroup::LeftArm:
		case EShooterHitGroup::RightArm:
			break;
	}
	return 1.0f;
}

void AShooterCharacter::ComputeArmorDamage(float Damage, float ArmorRatio, EShooterHitGroup HitGroup, bool bHelmet,
	float Armor, float& OutHealthDamage, float& OutArmorDamage)
{
	OutHealthDamage = Damage;
	OutArmorDamage = 0.0f;
	// CS: armor needs points and a weapon that respects it; it covers every group but the legs, the head only with a
	// helmet.
	const bool bLegs = HitGroup == EShooterHitGroup::LeftLeg || HitGroup == EShooterHitGroup::RightLeg;
	if (Armor <= 0.0f || ArmorRatio < 0.0f || bLegs || (HitGroup == EShooterHitGroup::Head && !bHelmet))
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

	// A point hit's group scales it (CS: the head x4, the stomach x1.25, the legs x0.75).
	EShooterHitGroup HitGroup = EShooterHitGroup::Generic;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		if (PointEvent.HitInfo.bBlockingHit)
		{
			HitGroup = GetHitGroup(PointEvent.HitInfo.ImpactPoint);
			Amount *= GetHitGroupMultiplier(HitGroup, GetCauserHeadshotMultiplier(DamageCauser));
		}
	}
	const bool bHeadshot = HitGroup == EShooterHitGroup::Head;

	float HealthDamage = 0.0f;
	float ArmorDamage = 0.0f;
	ComputeArmorDamage(
		Amount, GetCauserArmorRatio(DamageCauser), HitGroup, bHasHelmet, Armor, HealthDamage, ArmorDamage);
	Armor = FMath::Max(0.0f, Armor - ArmorDamage);
	const float Taken = FMath::Min(Health, HealthDamage);
	// The player sees where it came from (before a death lets the controller go).
	FVector SourceLocation;
	AShooterPlayerController* ShooterController = Cast<AShooterPlayerController>(GetController());
	if (ShooterController != nullptr && GetDamageSourceLocation(*this, EventInstigator, DamageCauser, SourceLocation))
	{
		ShooterController->NotifyTakeDamage(SourceLocation);
	}
	Health -= Taken;
	if (Health <= 0.0f)
	{
		Health = 0.0f;
		Die(EventInstigator != nullptr ? EventInstigator : GetController(), DamageCauser, bHeadshot);
	}
	else if (Taken > 0.0f && DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		// CS's tagging: a shot slows the victim.
		if (UShooterCharacterMovement* Move = GetShooterCharacterMovement())
		{
			Move->ApplyTagging();
		}
	}
	return Taken;
}

void AShooterCharacter::Flash(float HoldTime, float FadeTime, float Alpha, float BlindTime)
{
	const UWorld* World = GetWorld();
	const float Now = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	// A weaker flash than what is left of the last one changes nothing (CS's screen fade keeps the stronger).
	const float LastEnd = FlashStartTime + FlashHoldTime + FlashFadeTime;
	if (Alpha <= 0.0f || (GetFlashAlpha() >= Alpha && LastEnd >= Now + HoldTime + FadeTime))
	{
		return;
	}
	FlashStartTime = Now;
	FlashHoldTime = FMath::Max(0.0f, HoldTime);
	FlashFadeTime = FMath::Max(0.0f, FadeTime);
	FlashMaxAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	BlindEndTime = FMath::Max(BlindEndTime, Now + BlindTime);
	UE_LOG(LogShooter, Log, TEXT("%s is flashed: %.2f s white, %.2f s fading, blind %.2f s"), *GetName(),
		static_cast<double>(HoldTime), static_cast<double>(FadeTime), static_cast<double>(BlindTime));
}

float AShooterCharacter::GetFlashAlpha() const
{
	const UWorld* World = GetWorld();
	const float Elapsed = (World != nullptr ? World->GetTimeSeconds() : 0.0f) - FlashStartTime;
	if (Elapsed < 0.0f || FlashMaxAlpha <= 0.0f)
	{
		return 0.0f;
	}
	if (Elapsed < FlashHoldTime)
	{
		return FlashMaxAlpha;
	}
	const float Fading = Elapsed - FlashHoldTime;
	return Fading < FlashFadeTime ? FlashMaxAlpha * (1.0f - (Fading / FlashFadeTime)) : 0.0f;
}

bool AShooterCharacter::IsBlind() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetTimeSeconds() < BlindEndTime;
}

void AShooterCharacter::ClearFlash()
{
	FlashStartTime = -1.0e6f;
	FlashHoldTime = 0.0f;
	FlashFadeTime = 0.0f;
	FlashMaxAlpha = 0.0f;
	BlindEndTime = -1.0e6f;
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
	if (AShooterGameMode* GameMode = GetShooterGameMode())
	{
		GameMode->NotifyPawnsChanged();
		GameMode->Killed(Killer, Victim, this, DamageCauser, bHeadshot);
	}

	// Killed in the air (a jump, a fall): the movement stops below, so the body goes down to the floor now, and the
	// bomb and the weapon it drops with it (within a T's reach to pick up).
	if (World != nullptr && !IsMovingOnGround())
	{
		const FVector Feet = GetActorLocation();
		FCollisionQueryParams Params(FName(TEXT("DeathFloor")), false, this);
		FHitResult Floor;
		if (UGameplayStatics::LineTraceSingleByChannel(
				*World, Floor, Feet, Feet - FVector(0.0f, 0.0f, DeathFloorSearch), ECC_Visibility, Params))
		{
			(void)SetActorLocation(Floor.Location);
		}
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

	// The corpse: no collision (shots go through), no movement, the body falling: on its back when shot from the front,
	// on its front from behind (the killer's pawn, else what hit it), and lying there (the montage's last section
	// loops).
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement().Velocity = FVector::ZeroVector;
	GetCharacterMovement().SetComponentTickEnabled(false);
	if (HasSkeletalBody())
	{
		const AActor* Source = Killer != nullptr && Killer->GetPawn() != this ? Killer->GetPawn() : DamageCauser;
		const bool bFromBehind =
			Source != nullptr && ((Source->GetActorLocation() - GetActorLocation()) | GetActorForwardVector()) < 0.0f;
		UAnimMontage* Death = bFromBehind && DeathAnimFront != nullptr ? DeathAnimFront : DeathAnimBack;
		UAnimInstance& Anim = GetMesh().GetAnimInstance();
		Anim.Montage_Stop(0.0f);
		Anim.SetAimOffset(nullptr);
		if (Death == nullptr || Anim.Montage_Play(Death) <= 0.0f)
		{
			// A body without a death clip lies down as it is.
			GetMesh().SetRelativeLocationAndRotation(
				FVector(0.0f, 0.0f, ShooterCapsuleRadius * 0.5f), FRotator(90.0f, 0.0f, 0.0f));
		}
		GetMesh().MarkRenderStateDirty();
	}

	if (AShooterPlayerController* ShooterController = Cast<AShooterPlayerController>(Victim))
	{
		// CS's death cam: from the corpse's eyes toward the killer, then the teammates.
		ShooterController->StartDeathCam(Killer != nullptr && Killer != Victim ? Killer->GetPawn() : nullptr);
	}
	else if (Victim != nullptr)
	{
		Victim->UnPossess();
	}
	// Seen by everyone now, its own player included (the owner chain changed).
	GetMesh().MarkRenderStateDirty();
	if (CorpseLifeSpan > 0.0f)
	{
		SetLifeSpan(CorpseLifeSpan);
	}
}

// Inventory

void AShooterCharacter::SpawnDefaultInventory()
{
	GiveDefaultWeapons(DefaultWeapons);
	EquipBestWeapon();
}

void AShooterCharacter::SpawnTeamInventory()
{
	if (bTeamInventoryGiven || !IsAlive())
	{
		return;
	}
	bTeamInventoryGiven = true;
	GiveDefaultWeapons(GetTeam() == EShooterTeam::T ? DefaultWeaponsT : DefaultWeaponsCT);
	EquipBestWeapon();
}

void AShooterCharacter::GiveDefaultWeapons(const TArray<FString>& Names)
{
	for (const FString& WeaponName : Names)
	{
		UClass* WeaponClass = AShooterWeapon::FindWeaponClass(WeaponName);
		if (WeaponClass == nullptr)
		{
			UE_LOG(LogShooter, Warning, TEXT("%s: a default weapon '%s' is no weapon"), *GetName(), *WeaponName);
			continue;
		}
		if (AShooterWeapon* Weapon = GiveWeapon(WeaponClass))
		{
			(void)Weapon->GiveAmmo(DefaultWeaponClips * Weapon->AmmoPerClip);
		}
	}
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
	// One weapon a slot (the old one falls), but the grenade slot holds one of each grenade.
	AShooterWeapon* Old = Weapon->Slot == EShooterWeaponSlot::Grenade ? FindWeaponOfClass(Weapon->GetClass())
																	  : GetWeaponInSlot(Weapon->Slot);
	if (Old != nullptr && Old->CanBeDropped())
	{
		(void)DropWeapon(Old);
	}
	else if (Old != nullptr)
	{
		// A knife or a grenade given again replaces the one carried.
		RemoveWeapon(Old);
		(void)Old->Destroy();
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
	if (!IsAlive())
	{
		return;
	}
	if (Slot != EShooterWeaponSlot::Grenade)
	{
		EquipWeapon(GetWeaponInSlot(Slot));
		return;
	}
	// The grenades by their order; the one after the drawn grenade (wrapping), else the first.
	TArray<AShooterWeapon_Projectile*, TInlineAllocator<4>> Grenades;
	for (AShooterWeapon* Weapon : Inventory)
	{
		if (AShooterWeapon_Projectile* Grenade = Cast<AShooterWeapon_Projectile>(Weapon))
		{
			Grenades.Add(Grenade);
		}
	}
	if (Grenades.Num() == 0)
	{
		return;
	}
	Grenades.Sort([](const AShooterWeapon_Projectile& A, const AShooterWeapon_Projectile& B)
		{ return A.GrenadeOrder < B.GrenadeOrder; });
	const int32 Drawn = Grenades.IndexOfByKey(CurrentWeapon);
	EquipWeapon(Grenades[Drawn == INDEX_NONE ? 0 : (Drawn + 1) % Grenades.Num()]);
}

AShooterWeapon* AShooterCharacter::FindWeaponOfClass(const UClass* WeaponClass) const
{
	for (AShooterWeapon* Weapon : Inventory)
	{
		if (Weapon != nullptr && Weapon->GetClass() == WeaponClass)
		{
			return Weapon;
		}
	}
	return nullptr;
}

void AShooterCharacter::EquipBestWeapon()
{
	// A spent weapon is passed over (CS: an empty rifle gives way to the pistol); the bots call this every tick they
	// engage, so an empty primary must not win the slot order.
	AShooterWeapon* FirstOwned = nullptr;
	for (const EShooterWeaponSlot Slot : SlotsByPreference)
	{
		AShooterWeapon* Weapon = GetWeaponInSlot(Slot);
		if (Weapon == nullptr)
		{
			continue;
		}
		if (Weapon->HasAmmo())
		{
			EquipWeapon(Weapon);
			return;
		}
		if (FirstOwned == nullptr)
		{
			FirstOwned = Weapon;
		}
	}
	// Everything is empty: keep the weapon in hand, or draw one so the pawn holds something.
	if (CurrentWeapon == nullptr)
	{
		EquipWeapon(FirstOwned);
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
