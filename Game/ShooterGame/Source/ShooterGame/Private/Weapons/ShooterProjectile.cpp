#include "Weapons/ShooterProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ShooterGame.h"
#include "Sound/SoundWave.h"

namespace
{

	/** The grenade's sphere, cm. */
	constexpr float GrenadeRadius = 4.0f;

	/** The explosion's flash: a bright orange light for a moment. */
	constexpr float ExplosionLightIntensity = 12.0f;
	constexpr float ExplosionLightLifeSpan = 0.15f;

	/** The explosion's centre above where the grenade rests, so its line-of-sight tests leave the floor (cm). */
	constexpr float ExplosionLift = 10.0f;

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

void AShooterProjectile::BeginPlay()
{
	Super::BeginPlay();
	const UWorld* World = GetWorld();
	ExplodeTime = (World != nullptr ? World->GetTimeSeconds() : 0.0f) + FuseTime;
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
	const UWorld* World = GetWorld();
	ExplodeTime = (World != nullptr ? World->GetTimeSeconds() : 0.0f) + FuseTime;
}

void AShooterProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	if (!bExploded && World != nullptr && World->GetTimeSeconds() >= ExplodeTime)
	{
		Explode();
	}
}

void AShooterProjectile::Explode()
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;
	const FVector Origin = GetActorLocation() + FVector(0.0f, 0.0f, ExplosionLift);
	const TArray<AActor*> IgnoreActors;
	const bool bDamaged = UGameplayStatics::ApplyRadialDamageWithFalloff(this, ExplosionDamage, 0.0f, Origin, 0.0f,
		ExplosionRadius, 1.0f, UDamageType::StaticClass(), IgnoreActors, this, InstigatorController, ECC_Visibility);
	(void)UGameplayStatics::SpawnPointLightAtLocation(this, Origin, FLinearColor(1.0f, 0.6f, 0.25f),
		ExplosionLightIntensity, ExplosionRadius, ExplosionLightLifeSpan);
	if (ExplodeSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplodeSound, Origin);
	}
	UE_LOG(LogShooter, Log, TEXT("%s exploded at (%.0f, %.0f, %.0f)%s"), *GetName(), static_cast<double>(Origin.X),
		static_cast<double>(Origin.Y), static_cast<double>(Origin.Z), bDamaged ? TEXT(", damage done") : TEXT(""));
	(void)Destroy();
}
