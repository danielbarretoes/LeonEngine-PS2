#include "Weapons/ShooterProjectile.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterGameMode.h"
#include "ShooterPlayerController.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"
#include "Weapons/ShooterSmokeCloud.h"

namespace
{

	/** The grenade's sphere, cm. */
	constexpr float GrenadeRadius = 4.0f;

	/** The explosion's flash: a bright orange light for a moment. */
	constexpr float ExplosionLightIntensity = 12.0f;
	constexpr float ExplosionLightLifeSpan = 0.15f;

	/** The explosion's centre above where the grenade rests, so its line-of-sight tests leave the floor (cm). */
	constexpr float ExplosionLift = 10.0f;

	/** The flashbang's flash: a white light for a moment. */
	constexpr float FlashLightIntensity = 20.0f;
	constexpr float FlashLightRadius = 1500.0f;
	constexpr float FlashLightLifeSpan = 0.1f;

	/** How far below the smoke grenade its cloud's floor is looked for, cm. */
	constexpr float SmokeFloorSearch = 1000.0f;

} // namespace

AShooterProjectile::AShooterProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(AActor::DefaultSceneRootName))
{
	// UE ShooterGame: CollisionComp as the root. It blocks the world and the pawns (a grenade bounces off a player),
	// and the weapons' and the visibility traces pass through it.
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(GrenadeRadius);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);
	CollisionComp->SetCanEverAffectNavigation(false);
	CollisionComp->SetMobility(EComponentMobility::Movable);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetMobility(EComponentMobility::Movable);

	// A grenade: bounces, loses speed on each bounce, falls with the world's gravity.
	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	MovementComp->UpdatedComponent = CollisionComp;
	MovementComp->bShouldBounce = true;
	MovementComp->bRotationFollowsVelocity = false;
	MovementComp->Bounciness = 0.35f;
	MovementComp->Friction = 0.4f;
	MovementComp->ProjectileGravityScale = 1.0f;
}

void AShooterProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const UWorld* World = GetWorld();
	if (AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr)
	{
		GameMode->UnregisterProjectile(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AShooterProjectile::BeginPlay()
{
	Super::BeginPlay();
	const UWorld* World = GetWorld();
	if (AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr)
	{
		GameMode->RegisterProjectile(this);
	}
	GetWorldTimerManager().SetTimer(TimerHandle_Fuse, this, &AShooterProjectile::Explode, FuseTime);
}

void AShooterProjectile::Launch(const FVector& Velocity, AController* InInstigatorController, UStaticMesh* Mesh)
{
	MovementComp->Velocity = Velocity;
	InstigatorController = InInstigatorController;
	if (GetInstigator() != nullptr)
	{
		CollisionComp->IgnoreActorWhenMoving(GetInstigator(), true);
	}
	if (Mesh != nullptr)
	{
		(void)MeshComp->SetStaticMesh(Mesh);
	}
	// The fuse starts at the throw.
	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().SetTimer(TimerHandle_Fuse, this, &AShooterProjectile::Explode, FuseTime);
	}
}

float AShooterProjectile::GetExplodeTime() const
{
	const UWorld* World = GetWorld();
	const float Remaining = World != nullptr ? World->GetTimerManager().GetTimerRemaining(TimerHandle_Fuse) : -1.0f;
	return Remaining >= 0.0f ? World->GetTimeSeconds() + Remaining : 0.0f;
}

void AShooterProjectile::Explode()
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;
	Detonate(GetActorLocation() + FVector(0.0f, 0.0f, ExplosionLift));
	(void)Destroy();
}

void AShooterProjectile::Detonate(const FVector& Origin)
{
	const TArray<AActor*> IgnoreActors;
	const bool bDamaged = UGameplayStatics::ApplyRadialDamageWithFalloff(this, ExplosionDamage, 0.0f, Origin, 0.0f,
		ExplosionRadius, 1.0f, UDamageType::StaticClass(), IgnoreActors, this, InstigatorController, ECC_Visibility);
	(void)UGameplayStatics::SpawnPointLightAtLocation(this, Origin, FLinearColor(1.0f, 0.6f, 0.25f),
		ExplosionLightIntensity, ExplosionRadius, ExplosionLightLifeSpan);
	if (ExplodeSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplodeSound, Origin);
	}
	AShooterPlayerController::PlayExplosionForceFeedback(GetWorld(), Origin, ExplosionRadius);
	UE_LOG(LogShooter, Log, TEXT("%s exploded at (%.0f, %.0f, %.0f)%s"), *GetName(), static_cast<double>(Origin.X),
		static_cast<double>(Origin.Y), static_cast<double>(Origin.Z), bDamaged ? TEXT(", damage done") : TEXT(""));
}

// The flashbang

AShooterProjectile_Flashbang::AShooterProjectile_Flashbang(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool AShooterProjectile_Flashbang::ComputeFlash(const FVector& Origin, const FVector& EyeLocation,
	const FVector& ViewDirection, float& OutHold, float& OutFade, float& OutAlpha) const
{
	OutHold = 0.0f;
	OutFade = 0.0f;
	OutAlpha = 0.0f;
	const float Distance = FVector::Dist(Origin, EyeLocation);
	const float Strength = FlashRadius > 0.0f ? FlashStrength * (1.0f - (Distance / FlashRadius)) : 0.0f;
	if (Strength <= 0.0f)
	{
		return false;
	}
	// CS: the dot of the view's direction and the direction to the flash picks the band.
	const float Dot = ViewDirection.GetSafeNormal() | (Origin - EyeLocation).GetSafeNormal();
	const FVector& Band = Dot >= FacingDot ? FacingFlash : Dot >= AsideDot ? AsideFlash : BehindFlash;
	OutHold = Strength * Band.X;
	OutFade = Strength * Band.Y;
	OutAlpha = FMath::Clamp(Band.Z, 0.0f, 1.0f);
	return true;
}

void AShooterProjectile_Flashbang::Detonate(const FVector& Origin)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	// The living players, the game mode's registry (else the world's, in a test without one).
	TArray<AShooterCharacter*, TInlineAllocator<16>> Players;
	if (const AShooterGameMode* GameMode = World->GetAuthGameMode<AShooterGameMode>())
	{
		Players.Append(GameMode->GetPawns());
	}
	else
	{
		World->ForEach<AShooterCharacter>([&Players](AShooterCharacter& Pawn) { Players.Add(&Pawn); });
	}
	int32 NumFlashed = 0;
	for (AShooterCharacter* Pawn : Players)
	{
		if (Pawn == nullptr || Pawn->IsPendingKillPending() || !Pawn->IsAlive())
		{
			continue;
		}
		const FVector Eyes = Pawn->GetFirstPersonCameraComponent()->GetComponentLocation();
		float Hold = 0.0f;
		float Fade = 0.0f;
		float Alpha = 0.0f;
		if (!ComputeFlash(Origin, Eyes, Pawn->GetViewRotation().Vector(), Hold, Fade, Alpha))
		{
			continue;
		}
		// The explosion must see the eyes: a wall hides them (the pawns do not block the Visibility channel).
		FCollisionQueryParams Params(FName(TEXT("Flashbang")), false, this);
		Params.AddIgnoredActor(Pawn);
		FHitResult Hit;
		if (UGameplayStatics::LineTraceSingleByChannel(*World, Hit, Origin, Eyes, ECC_Visibility, Params))
		{
			continue;
		}
		Pawn->Flash(Hold, Fade, Alpha, Fade * BotBlindFadeShare);
		++NumFlashed;
	}
	(void)UGameplayStatics::SpawnPointLightAtLocation(
		this, Origin, FLinearColor(1.0f, 1.0f, 1.0f), FlashLightIntensity, FlashLightRadius, FlashLightLifeSpan);
	if (ExplodeSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplodeSound, Origin);
	}
	// The bang shakes the pads near it, as an HE's (ps2-shipping N24).
	AShooterPlayerController::PlayExplosionForceFeedback(World, Origin, FlashRadius * 0.5f);
	UE_LOG(LogShooter, Log, TEXT("%s flashed %d player(s)"), *GetName(), NumFlashed);
}

// The smoke grenade

AShooterProjectile_Smoke::AShooterProjectile_Smoke(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SmokeCloudClass = AShooterSmokeCloud::StaticClass();
}

void AShooterProjectile_Smoke::Detonate(const FVector& Origin)
{
	UWorld* World = GetWorld();
	if (World == nullptr || SmokeCloudClass == nullptr)
	{
		return;
	}
	// The cloud stands on the floor under the grenade (CS's smoke rises from where it lies).
	FVector Floor = Origin;
	FCollisionQueryParams Params(FName(TEXT("SmokeFloor")), false, this);
	FHitResult Hit;
	if (UGameplayStatics::LineTraceSingleByChannel(
			*World, Hit, Origin, Origin - FVector(0.0f, 0.0f, SmokeFloorSearch), ECC_Visibility, Params))
	{
		Floor = Hit.ImpactPoint;
	}
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	(void)World->SpawnActor<AShooterSmokeCloud>(SmokeCloudClass, Floor, FRotator::ZeroRotator, SpawnInfo);
	if (ExplodeSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplodeSound, Origin);
	}
	UE_LOG(LogShooter, Log, TEXT("%s: smoke at (%.0f, %.0f, %.0f)"), *GetName(), static_cast<double>(Floor.X),
		static_cast<double>(Floor.Y), static_cast<double>(Floor.Z));
}
