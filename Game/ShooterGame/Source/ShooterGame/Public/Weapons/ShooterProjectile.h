#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterProjectile.generated.h"

class AController;
class UProjectileMovementComponent;
class USoundWave;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A thrown grenade (UE ShooterGame: AShooterProjectile, whose rocket explodes on impact; Counter-Strike's HE grenade
 * explodes on a fuse): a small sphere that flies and bounces (UProjectileMovementComponent) and explodes FuseTime
 * seconds after the throw.
 *
 * The explosion (Explode) is UE's radial damage with a line of sight
 * (UGameplayStatics::ApplyRadialDamageWithFalloff on the Visibility channel, so a wall shields; pawns do not block it):
 * ExplosionDamage at the centre falling linearly to nothing at ExplosionRadius, the thrower's controller the
 * instigator and the projectile the causer (its ArmorRatio is the victims' armor rule). The thrower is hurt too (CS).
 * A short bright light and ExplodeSound mark it; then the projectile is destroyed.
 *
 * The launching weapon (AShooterWeapon_Projectile::FireWeapon) sets the velocity, the fuse, the damage and the look.
 */
UCLASS()
class SHOOTERGAME_API AShooterProjectile : public AActor
{
	GENERATED_BODY()

public:
	AShooterProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Seconds from the throw to the explosion (CS's HE: 1.5). */
	UPROPERTY()
	float FuseTime = 1.5f;

	/** The damage at the centre (CS's HE: 98). */
	UPROPERTY()
	float ExplosionDamage = 98.0f;

	/** Where the damage ends, cm (CS's HE: 350 units). */
	UPROPERTY()
	float ExplosionRadius = 889.0f;

	/** The victims' armor rule for this damage (AShooterWeapon::ArmorRatio). */
	UPROPERTY()
	float ArmorRatio = 1.0f;

	/** The explosion's sound (null: silent). */
	UPROPERTY(Transient)
	USoundWave* ExplodeSound = nullptr;

	/**
	 * Throws the projectile: its velocity, the controller credited with the damage and the mesh it shows (UE
	 * ShooterGame: InitVelocity).
	 */
	void Launch(const FVector& Velocity, AController* InInstigatorController, UStaticMesh* Mesh);

	/** Explodes now (the fuse, or a test); a projectile explodes once. */
	void Explode();
	[[nodiscard]] bool HasExploded() const
	{
		return bExploded;
	}
	/** The world time the fuse runs out. */
	[[nodiscard]] float GetExplodeTime() const
	{
		return ExplodeTime;
	}

	[[nodiscard]] UProjectileMovementComponent* GetMovementComponent() const
	{
		return MovementComp;
	}

	void BeginPlay() override;
	void Tick(float DeltaSeconds) override;

private:
	/** UE ShooterGame: CollisionComp, the root. */
	UPROPERTY()
	USphereComponent* CollisionComp = nullptr;

	UPROPERTY()
	UStaticMeshComponent* MeshComp = nullptr;

	/** UE ShooterGame: MovementComp. */
	UPROPERTY()
	UProjectileMovementComponent* MovementComp = nullptr;

	/** The thrower's controller (UE ShooterGame: MyController). */
	UPROPERTY(Transient)
	AController* InstigatorController = nullptr;

	float ExplodeTime = 0.0f;
	bool bExploded = false;
};
