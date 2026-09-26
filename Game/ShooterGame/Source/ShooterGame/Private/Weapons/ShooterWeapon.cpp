#include "Weapons/ShooterWeapon.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "Sound/SoundWave.h"
#include "UObject/UObjectHash.h"

namespace
{

	/** The asset a config path names, when its package exists (a missing asset is no error: the weapon is silent). */
	template <class T>
	T* LoadOptionalAsset(const FSoftObjectPath& Path)
	{
		if (Path.IsNull() || !FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{
			return nullptr;
		}
		return Cast<T>(Path.TryLoad());
	}

	/** The muzzle flash: a short orange light (UGameplayStatics::SpawnPointLightAtLocation). */
	constexpr float MuzzleFlashIntensity = 3.0f;
	constexpr float MuzzleFlashRadius = 400.0f;
	constexpr float MuzzleFlashLifeSpan = 0.05f;

	/** How early a shot may come, seconds: the world's time sums float frame times (6 x 1/60 s may miss 0.1 s). */
	constexpr float FireTimeTolerance = 1.0e-3f;

} // namespace

const FName AShooterWeapon::MuzzleSocketName(TEXT("Muzzle"));

AShooterWeapon::AShooterWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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
	if (UStaticMesh* Mesh = LoadOptionalAsset<UStaticMesh>(MeshName))
	{
		(void)Mesh1P->SetStaticMesh(Mesh);
		(void)Mesh3P->SetStaticMesh(Mesh);
	}
	FireSound = LoadOptionalAsset<USoundWave>(FireSoundName);
	ReloadSound = LoadOptionalAsset<USoundWave>(ReloadSoundName);
	EmptySound = LoadOptionalAsset<USoundWave>(EmptySoundName);
	EquipSound = LoadOptionalAsset<USoundWave>(EquipSoundName);
	RefillAmmo();
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
	TArray<UClass*> Classes;
	GetDerivedClasses(AShooterWeapon::StaticClass(), Classes, /*bRecursive=*/true);
	for (UClass* Class : Classes)
	{
		if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		const AShooterWeapon* Defaults = Class->GetDefaultObject<AShooterWeapon>();
		if (Defaults->WeaponName.Equals(Name, ESearchCase::IgnoreCase) ||
			Class->GetName().EndsWith(TEXT("_") + Name, ESearchCase::IgnoreCase))
		{
			return Class;
		}
	}
	return nullptr;
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

int32 AShooterWeapon::GiveAmmo(int32 AddAmount)
{
	const int32 Taken = FMath::Clamp(AddAmount, 0, FMath::Max(0, MaxAmmo - CurrentAmmo));
	CurrentAmmo += Taken;
	return Taken;
}

void AShooterWeapon::OnEnterInventory(AShooterCharacter* NewOwner)
{
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
}

void AShooterWeapon::TickPickup()
{
	const UWorld* World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr || GetWorldTime() < PickupTime)
	{
		return;
	}
	constexpr float PickupReachZ = 100.0f;
	const FVector Location = GetActorLocation();
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		AShooterCharacter* Pawn = Cast<AShooterCharacter>(Actor);
		if (Pawn == nullptr || Pawn->IsPendingKillPending() || !Pawn->IsAlive() ||
			Pawn->GetWeaponInSlot(Slot) != nullptr)
		{
			continue;
		}
		const FVector Delta = Pawn->GetActorLocation() - Location;
		if (Delta.SizeSquared2D() <= FMath::Square(PickupRadius) && FMath::Abs(Delta.Z) <= PickupReachZ)
		{
			UE_LOG(LogShooter, Log, TEXT("%s picked up %s"), *Pawn->GetName(), *WeaponName);
			Pawn->AddWeapon(this);
			return;
		}
	}
}

void AShooterWeapon::OnEquip()
{
	bIsEquipped = true;
	bWantsToFire = false;
	bFiredThisPress = false;
	CurrentState = EShooterWeaponState::Equipping;
	EquipFinishTime = GetWorldTime() + EquipDuration;
	AttachMeshToPawn();
	PlayWeaponSound(EquipSound);
}

void AShooterWeapon::OnUnEquip()
{
	bIsEquipped = false;
	StopFire();
	StopReload();
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
	if (Mesh1P->GetAttachParent() != MyPawn->GetFirstPersonCameraComponent())
	{
		(void)Mesh1P->AttachToComponent(
			MyPawn->GetFirstPersonCameraComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}
	Mesh1P->SetRelativeLocationAndRotation(FirstPersonOffset, FRotator::ZeroRotator);
	if (Mesh3P->GetAttachParent() != MyPawn->GetBodyMesh())
	{
		(void)Mesh3P->AttachToComponent(MyPawn->GetBodyMesh(), FAttachmentTransformRules::KeepRelativeTransform);
	}
	Mesh3P->SetRelativeLocationAndRotation(ThirdPersonOffset, FRotator::ZeroRotator);
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
		if (CanFire() && GetWorldTime() + FireTimeTolerance >= LastFireTime + TimeBetweenShots)
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
	return bIsEquipped && MyPawn != nullptr && MyPawn->IsAlive() &&
		(CurrentState == EShooterWeaponState::Idle || CurrentState == EShooterWeaponState::Firing);
}

bool AShooterWeapon::CanReload() const
{
	return bIsEquipped && CurrentAmmoInClip < AmmoPerClip && CurrentAmmo > 0 &&
		CurrentState != EShooterWeaponState::Reloading && CurrentState != EShooterWeaponState::Equipping;
}

void AShooterWeapon::StartReload()
{
	if (!CanReload())
	{
		return;
	}
	CurrentState = EShooterWeaponState::Reloading;
	ReloadFinishTime = GetWorldTime() + ReloadDuration;
	PlayWeaponSound(ReloadSound);
}

void AShooterWeapon::StopReload()
{
	if (CurrentState == EShooterWeaponState::Reloading)
	{
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
	CurrentAmmoInClip = FMath::Max(0, CurrentAmmoInClip - 1);
}

void AShooterWeapon::HandleFiring()
{
	if (CurrentAmmoInClip > 0 && CanFire())
	{
		CurrentState = EShooterWeaponState::Firing;
		FireWeapon();
		UseAmmo();
		++ShotsFired;
		LastFireTime = GetWorldTime();
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
	PlayWeaponSound(FireSound);
	(void)UGameplayStatics::SpawnPointLightAtLocation(this, GetMuzzleLocation(), FLinearColor(1.0f, 0.72f, 0.35f),
		MuzzleFlashIntensity, MuzzleFlashRadius, MuzzleFlashLifeSpan);
}

void AShooterWeapon::PlayWeaponSound(USoundWave* Sound) const
{
	if (Sound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
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
	if (CurrentState == EShooterWeaponState::Equipping && Now + FireTimeTolerance >= EquipFinishTime)
	{
		CurrentState = EShooterWeaponState::Idle;
	}
	if (CurrentState == EShooterWeaponState::Reloading && Now + FireTimeTolerance >= ReloadFinishTime)
	{
		ReloadWeapon();
		CurrentState = EShooterWeaponState::Idle;
	}
	// The trigger held: the next shot when the fire rate allows (automatic), or the press's first shot once ready.
	if (bWantsToFire && CanFire() && Now + FireTimeTolerance >= LastFireTime + TimeBetweenShots &&
		(bAutomatic || !bFiredThisPress))
	{
		HandleFiring();
	}
}
