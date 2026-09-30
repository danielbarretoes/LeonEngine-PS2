#include "Weapons/ShooterWeapon.h"

#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpaceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterGameMode.h"
#include "ShooterPlayerController.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"
#include "UObject/UObjectHash.h"

namespace
{

	/** The muzzle flash: a short orange light (UGameplayStatics::SpawnPointLightAtLocation). */
	constexpr float MuzzleFlashIntensity = 3.0f;
	constexpr float MuzzleFlashRadius = 400.0f;
	constexpr float MuzzleFlashLifeSpan = 0.05f;

	/** How early a shot may come, seconds: the world's time sums float frame times (6 x 1/60 s may miss 0.1 s). */
	constexpr float FireTimeTolerance = 1.0e-3f;

	/** A weapon class FindWeaponClass can name, and its class name. */
	struct FWeaponClassEntry
	{
		UClass* Class = nullptr;
		FString ClassName;
	};

	/** The concrete weapon classes, listed once (the classes are native: they live as long as the program). */
	const TArray<FWeaponClassEntry>& ListWeaponClasses()
	{
		static const TArray<FWeaponClassEntry> Entries = []()
		{
			TArray<UClass*> Classes;
			GetDerivedClasses(AShooterWeapon::StaticClass(), Classes, /*bRecursive=*/true);
			TArray<FWeaponClassEntry> Found;
			for (UClass* Class : Classes)
			{
				if (!Class->HasAnyClassFlags(CLASS_Abstract))
				{
					Found.Add(FWeaponClassEntry{Class, Class->GetName()});
				}
			}
			return Found;
		}();
		return Entries;
	}

	/** ClassName ends with `_` and Name, any case (ShooterWeapon_AK47 and "ak47"). */
	bool HasClassSuffix(const FString& ClassName, const FString& Name)
	{
		const int32 Underscore = ClassName.Len() - Name.Len() - 1;
		return Underscore >= 0 && ClassName[Underscore] == TEXT('_') &&
			ClassName.EndsWith(Name, ESearchCase::IgnoreCase);
	}

	/** The world's game mode as the game's class (its registries), null without one. */
	AShooterGameMode* GetShooterGameMode(const UWorld* World)
	{
		return World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	}

} // namespace

const FName AShooterWeapon::MuzzleSocketName(TEXT("Muzzle"));

AShooterWeapon::AShooterWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	// The view model: drawn in its owner's view only, last, with the camera's view model field of view.
	Mesh1P = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh1P"));
	Mesh1P->SetupAttachment(GetRootComponent());
	Mesh1P->bRenderAsViewModel = true;
	Mesh1P->bOnlyOwnerSee = true;
	Mesh1P->CastShadow = false;
	Mesh1P->SetMobility(EComponentMobility::Movable);
	Mesh1P->SetVisibility(false);

	// What the others see, on the body.
	Mesh3P = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh3P"));
	Mesh3P->SetupAttachment(GetRootComponent());
	Mesh3P->bOwnerNoSee = true;
	Mesh3P->SetMobility(EComponentMobility::Movable);
	Mesh3P->SetVisibility(false);
}

void AShooterWeapon::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	UStaticMesh* WorldMesh = LoadShooterAsset<UStaticMesh>(MeshName);
	UStaticMesh* ViewMesh = LoadShooterAsset<UStaticMesh>(FirstPersonMeshName);
	if (WorldMesh != nullptr)
	{
		(void)Mesh3P->SetStaticMesh(WorldMesh);
	}
	if (ViewMesh != nullptr || WorldMesh != nullptr)
	{
		(void)Mesh1P->SetStaticMesh(ViewMesh != nullptr ? ViewMesh : WorldMesh);
	}
	ArmsIdle = LoadShooterAsset<UBlendSpaceBase>(ArmsIdleName);
	AimOffset = LoadShooterAsset<UAimOffsetBlendSpace1D>(AimOffsetName);
	FireSound = LoadShooterAsset<USoundWave>(FireSoundName);
	ReloadSound = LoadShooterAsset<USoundWave>(ReloadSoundName);
	EmptySound = LoadShooterAsset<USoundWave>(EmptySoundName);
	EquipSound = LoadShooterAsset<USoundWave>(EquipSoundName);
	MagOutSound = LoadShooterAsset<USoundWave>(MagOutSoundName);
	MagInSound = LoadShooterAsset<USoundWave>(MagInSoundName);
	FireAnim = {LoadShooterAsset<UAnimMontage>(FireAnim1PName), LoadShooterAsset<UAnimMontage>(FireAnim3PName)};
	ReloadAnim = {LoadShooterAsset<UAnimMontage>(ReloadAnim1PName), LoadShooterAsset<UAnimMontage>(ReloadAnim3PName)};
	EquipAnim = {LoadShooterAsset<UAnimMontage>(EquipAnim1PName), LoadShooterAsset<UAnimMontage>(EquipAnim3PName)};
	// CS 1.6: a new weapon comes with a full clip; its reserve is bought by the box.
	CurrentAmmoInClip = AmmoPerClip;
	CurrentAmmo = 0;
}

UStaticMesh* AShooterWeapon::GetWeaponMesh() const
{
	return Mesh3P->GetStaticMesh();
}

UClass* AShooterWeapon::FindWeaponClass(const FString& Name)
{
	if (Name.IsEmpty())
	{
		return nullptr;
	}
	for (const FWeaponClassEntry& Entry : ListWeaponClasses())
	{
		const AShooterWeapon* Defaults = Entry.Class->GetDefaultObject<AShooterWeapon>();
		if (Defaults->WeaponName.Equals(Name, ESearchCase::IgnoreCase) || HasClassSuffix(Entry.ClassName, Name))
		{
			return Entry.Class;
		}
	}
	return nullptr;
}

void AShooterWeapon::GetWeaponClasses(TArray<UClass*>& OutClasses)
{
	OutClasses.Reset();
	for (const FWeaponClassEntry& Entry : ListWeaponClasses())
	{
		OutClasses.Add(Entry.Class);
	}
}

AController* AShooterWeapon::GetInstigatorController() const
{
	return MyPawn != nullptr ? MyPawn->GetController() : nullptr;
}

float AShooterWeapon::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.0f;
}

void AShooterWeapon::RefillAmmo()
{
	CurrentAmmoInClip = AmmoPerClip;
	CurrentAmmo = MaxAmmo;
}

void AShooterWeapon::SetAmmo(int32 NewAmmoInClip, int32 NewAmmo)
{
	CurrentAmmoInClip = FMath::Clamp(NewAmmoInClip, 0, AmmoPerClip);
	CurrentAmmo = FMath::Clamp(NewAmmo, 0, MaxAmmo);
}

int32 AShooterWeapon::GiveAmmo(int32 AddAmount)
{
	const int32 Taken = FMath::Clamp(AddAmount, 0, FMath::Max(0, MaxAmmo - CurrentAmmo));
	CurrentAmmo += Taken;
	return Taken;
}

void AShooterWeapon::OnEnterInventory(AShooterCharacter* NewOwner)
{
	if (bDropped)
	{
		if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
		{
			GameMode->UnregisterPickup(this);
		}
	}
	bDropped = false;
	MyPawn = NewOwner;
	SetOwner(NewOwner);
	SetInstigator(NewOwner);
	// The meshes' owner chain changed (bOnlyOwnerSee / bOwnerNoSee read it when their proxies are made).
	Mesh1P->MarkRenderStateDirty();
	Mesh3P->MarkRenderStateDirty();
	AttachMeshToPawn();
}

void AShooterWeapon::OnLeaveInventory()
{
	if (bIsEquipped)
	{
		OnUnEquip();
	}
	DetachMeshFromPawn();
	MyPawn = nullptr;
	SetOwner(nullptr);
	SetInstigator(nullptr);
	Mesh1P->MarkRenderStateDirty();
	Mesh3P->MarkRenderStateDirty();
}

void AShooterWeapon::OnDropped(const FVector& Location, float Yaw)
{
	bDropped = true;
	PickupTime = GetWorldTime() + PickupDelay;
	// Lying on its side on the floor (the mesh's forward along the drop's yaw).
	(void)SetActorLocationAndRotation(Location, FRotator(0.0f, Yaw, 0.0f));
	Mesh3P->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, 2.0f), FRotator(0.0f, 0.0f, 90.0f));
	Mesh3P->SetVisibility(true);
	if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
	{
		GameMode->RegisterPickup(this);
	}
}

void AShooterWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
	{
		GameMode->UnregisterPickup(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AShooterWeapon::TickPickup()
{
	const AShooterGameMode* GameMode = GetShooterGameMode(GetWorld());
	if (GameMode == nullptr || GetWorldTime() < PickupTime)
	{
		return;
	}
	constexpr float PickupReachZ = 100.0f;
	const FVector Location = GetActorLocation();
	// The game mode's pawns, in the level's order (AddWeapon changes no registry).
	for (AShooterCharacter* Pawn : GameMode->GetPawns())
	{
		if (Pawn->IsPendingKillPending() || !Pawn->IsAlive() || Pawn->GetWeaponInSlot(Slot) != nullptr)
		{
			continue;
		}
		const FVector Delta = Pawn->GetActorLocation() - Location;
		if (Delta.SizeSquared2D() <= FMath::Square(PickupRadius) && FMath::Abs(Delta.Z) <= PickupReachZ)
		{
			UE_LOG(LogShooter, Log, TEXT("%s picked up %s"), *Pawn->GetName(), *WeaponName);
			Pawn->PickUpWeapon(this);
			return;
		}
	}
}

void AShooterWeapon::OnEquip()
{
	bIsEquipped = true;
	bWantsToFire = false;
	bFiredThisPress = false;
	AttachMeshToPawn();
	if (MyPawn != nullptr)
	{
		MyPawn->SetArmsIdle(ArmsIdle);
	}
	// The draw lasts its montage when it has one (UE ShooterGame), EquipDuration otherwise.
	const float AnimDuration = PlayWeaponAnimation(EquipAnim);
	SetEquippingFor(AnimDuration > 0.0f ? AnimDuration : EquipDuration);
	PlayEquipSound();
}

void AShooterWeapon::SetEquippingFor(float Seconds)
{
	CurrentState = EShooterWeaponState::Equipping;
	GetWorldTimerManager().SetTimer(
		TimerHandle_OnEquipFinished, this, &AShooterWeapon::OnEquipFinished, FMath::Max(Seconds, 0.01f));
}

void AShooterWeapon::OnUnEquip()
{
	bIsEquipped = false;
	StopFire();
	StopReload();
	StopWeaponAnimation(EquipAnim);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_OnEquipFinished);
	}
	CurrentState = EShooterWeaponState::Idle;
	AttachMeshToPawn();
}

void AShooterWeapon::AttachMeshToPawn()
{
	if (MyPawn == nullptr)
	{
		return;
	}
	USceneComponent* Root = GetRootComponent();
	if (Root->GetAttachParent() != MyPawn->GetRootComponent())
	{
		(void)Root->AttachToComponent(
			MyPawn->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	// On the arms' and the body's hand sockets when the pawn has them; else at the offsets from the camera and the
	// static body.
	FName Socket1P;
	USceneComponent* Parent1P = MyPawn->GetWeaponAttachParent1P(Socket1P);
	if (Mesh1P->GetAttachParent() != Parent1P || Mesh1P->GetAttachSocketName() != Socket1P)
	{
		(void)Mesh1P->AttachToComponent(Parent1P, FAttachmentTransformRules::KeepRelativeTransform, Socket1P);
	}
	Mesh1P->SetRelativeLocationAndRotation(
		Socket1P.IsNone() ? FirstPersonOffset : FVector::ZeroVector, FRotator::ZeroRotator);
	FName Socket3P;
	USceneComponent* Parent3P = MyPawn->GetWeaponAttachParent3P(Socket3P);
	if (Mesh3P->GetAttachParent() != Parent3P || Mesh3P->GetAttachSocketName() != Socket3P)
	{
		(void)Mesh3P->AttachToComponent(Parent3P, FAttachmentTransformRules::KeepRelativeTransform, Socket3P);
	}
	Mesh3P->SetRelativeLocationAndRotation(
		Socket3P.IsNone() ? ThirdPersonOffset : FVector::ZeroVector, FRotator::ZeroRotator);
	Mesh1P->SetVisibility(bIsEquipped);
	Mesh3P->SetVisibility(bIsEquipped);
}

void AShooterWeapon::DetachMeshFromPawn()
{
	const FDetachmentTransformRules KeepWorld = FDetachmentTransformRules::KeepWorldTransform;
	Mesh1P->DetachFromComponent(KeepWorld);
	Mesh3P->DetachFromComponent(KeepWorld);
	GetRootComponent()->DetachFromComponent(KeepWorld);
	(void)Mesh1P->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	(void)Mesh3P->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Mesh1P->SetVisibility(false);
	Mesh3P->SetVisibility(false);
}

void AShooterWeapon::StartFire()
{
	if (!bWantsToFire)
	{
		bWantsToFire = true;
		bFiredThisPress = false;
		// The first shot of a press fires at once when the weapon is ready (the tick takes the next ones).
		if (CanFire() && GetWorldTime() + FireTimeTolerance >= LastFireTime + GetTimeBetweenShots())
		{
			HandleFiring();
		}
	}
}

void AShooterWeapon::StopFire()
{
	bWantsToFire = false;
	bFiredThisPress = false;
	if (CurrentState == EShooterWeaponState::Firing)
	{
		CurrentState = EShooterWeaponState::Idle;
	}
}

bool AShooterWeapon::CanFire() const
{
	// Frozen (the freeze, the match's end): a trigger held from before fires no more.
	return bIsEquipped && MyPawn != nullptr && MyPawn->IsAlive() && !MyPawn->IsFrozen() &&
		(CurrentState == EShooterWeaponState::Idle || CurrentState == EShooterWeaponState::Firing);
}

bool AShooterWeapon::CanReload() const
{
	return bIsEquipped && !bInfiniteClip && CurrentAmmoInClip < AmmoPerClip && CurrentAmmo > 0 &&
		CurrentState != EShooterWeaponState::Reloading && CurrentState != EShooterWeaponState::Equipping;
}

void AShooterWeapon::StartReload()
{
	if (!CanReload())
	{
		return;
	}
	CurrentState = EShooterWeaponState::Reloading;
	// The reload lasts CS's ReloadDuration, its montage fitted to it (weapons share the clips of their kind).
	(void)PlayWeaponAnimation(ReloadAnim, ReloadDuration);
	GetWorldTimerManager().SetTimer(
		TimerHandle_ReloadWeapon, this, &AShooterWeapon::OnReloadFinished, FMath::Max(ReloadDuration, 0.01f));
	PlayWeaponSound(ReloadSound);
}

void AShooterWeapon::StopReload()
{
	if (CurrentState == EShooterWeaponState::Reloading)
	{
		CurrentState = EShooterWeaponState::Idle;
		StopWeaponAnimation(ReloadAnim);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_ReloadWeapon);
	}
}

float AShooterWeapon::PlayWeaponAnimation(const FShooterWeaponAnim& Animation, float Duration)
{
	if (MyPawn == nullptr)
	{
		return 0.0f;
	}
	float PlayRate = 1.0f;
	if (Duration > 0.0f)
	{
		// The rate that makes the longer montage last Duration.
		const float Length = FMath::Max(Animation.Pawn1P != nullptr ? Animation.Pawn1P->GetPlayLength() : 0.0f,
			Animation.Pawn3P != nullptr ? Animation.Pawn3P->GetPlayLength() : 0.0f);
		PlayRate = Length > 0.0f ? Length / Duration : 1.0f;
	}
	return MyPawn->PlayPawnMontages(Animation, PlayRate);
}

void AShooterWeapon::StopWeaponAnimation(const FShooterWeaponAnim& Animation)
{
	if (MyPawn != nullptr)
	{
		MyPawn->StopPawnMontages(Animation);
	}
}

void AShooterWeapon::OnAnimNotify(FName NotifyName)
{
	static const FName MagOutName(TEXT("MagOut"));
	static const FName MagInName(TEXT("MagIn"));
	if (NotifyName == MagOutName)
	{
		PlayWeaponSound(MagOutSound);
	}
	else if (NotifyName == MagInName)
	{
		PlayWeaponSound(MagInSound);
	}
}

void AShooterWeapon::OnEquipFinished()
{
	if (CurrentState == EShooterWeaponState::Equipping)
	{
		CurrentState = EShooterWeaponState::Idle;
	}
}

void AShooterWeapon::OnReloadFinished()
{
	if (CurrentState == EShooterWeaponState::Reloading)
	{
		ReloadWeapon();
		CurrentState = EShooterWeaponState::Idle;
	}
}

void AShooterWeapon::ReloadWeapon()
{
	const int32 ClipDelta = FMath::Min(AmmoPerClip - CurrentAmmoInClip, CurrentAmmo);
	CurrentAmmoInClip += ClipDelta;
	CurrentAmmo -= ClipDelta;
}

void AShooterWeapon::UseAmmo()
{
	if (!bInfiniteClip)
	{
		CurrentAmmoInClip = FMath::Max(0, CurrentAmmoInClip - 1);
	}
}

void AShooterWeapon::HandleFiring()
{
	if ((bInfiniteClip || CurrentAmmoInClip > 0) && CanFire())
	{
		// A held automatic trigger keeps the weapon's cadence at any frame rate: the part of the frame past the shot's
		// time counts toward the next one (UE ShooterGame: TimerIntervalAdjustment), so 30 fps fires as fast as 60.
		const float Now = GetWorldTime();
		const float Cycle = GetTimeBetweenShots();
		const float Due = LastFireTime + Cycle;
		const bool bRefiring = bAutomatic && bFiredThisPress && Now >= Due - FireTimeTolerance && Now < Due + Cycle;
		CurrentState = EShooterWeaponState::Firing;
		FireWeapon();
		UseAmmo();
		++ShotsFired;
		LastFireTime = bRefiring ? Due : Now;
		bFiredThisPress = true;
		SimulateWeaponFire();
		OnShotFired();
		return;
	}
	// CS: an empty clip reloads, an empty weapon clicks.
	bFiredThisPress = true;
	if (CanReload())
	{
		StartReload();
		return;
	}
	if (CurrentAmmoInClip == 0 && CurrentAmmo == 0 && bIsEquipped)
	{
		PlayWeaponSound(EmptySound);
		LastFireTime = GetWorldTime();
	}
}

void AShooterWeapon::SimulateWeaponFire()
{
	// The bots hear shots (AActor::MakeNoise, UPawnSensingComponent).
	MakeNoise(GetFireNoiseLoudness(), MyPawn, GetActorLocation());
	PlayWeaponSound(FireSound, GetFireVolume());
	PlayFireForceFeedback();
	(void)PlayWeaponAnimation(FireAnim);
	(void)UGameplayStatics::SpawnPointLightAtLocation(this, GetMuzzleLocation(), FLinearColor(1.0f, 0.72f, 0.35f),
		MuzzleFlashIntensity, MuzzleFlashRadius, MuzzleFlashLifeSpan);
}

void AShooterWeapon::PlayFireForceFeedback() const
{
	AShooterPlayerController* Controller =
		MyPawn != nullptr ? Cast<AShooterPlayerController>(MyPawn->GetController()) : nullptr;
	if (Controller != nullptr)
	{
		Controller->PlayFireForceFeedback();
	}
}

void AShooterWeapon::PlayWeaponSound(USoundWave* Sound, float VolumeMultiplier) const
{
	if (Sound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), VolumeMultiplier);
	}
}

void AShooterWeapon::GetAim(FVector& OutStart, FVector& OutDirection) const
{
	if (MyPawn != nullptr)
	{
		OutStart = MyPawn->GetFirstPersonCameraComponent()->GetComponentLocation();
		OutDirection = MyPawn->GetViewRotation().Vector();
		return;
	}
	OutStart = GetActorLocation();
	OutDirection = GetActorForwardVector();
}

FVector AShooterWeapon::GetMuzzleLocation() const
{
	// The first-person muzzle for the player who sees it, the body's for the others (UE ShooterGame: GetWeaponMesh).
	const UStaticMeshComponent* Mesh =
		Mesh1P->IsVisible() && MyPawn != nullptr && MyPawn->IsFirstPerson() ? Mesh1P : Mesh3P;
	if (Mesh->DoesSocketExist(MuzzleSocketName))
	{
		return Mesh->GetSocketTransform(MuzzleSocketName).GetLocation();
	}
	return Mesh->GetComponentTransform().TransformPosition(MuzzleOffset);
}

void AShooterWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDropped)
	{
		TickPickup();
		return;
	}
	const float Now = GetWorldTime();
	// The trigger held: the next shot when the fire rate allows (automatic), or the press's first shot once ready.
	if (bWantsToFire && CanFire() && Now + FireTimeTolerance >= LastFireTime + GetTimeBetweenShots() &&
		(bAutomatic || !bFiredThisPress))
	{
		HandleFiring();
	}
}
