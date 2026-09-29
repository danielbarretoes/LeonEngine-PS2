#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterProjectile.generated.h"

class AController;
class AShooterSmokeCloud;
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
 * The explosion (Explode, then Detonate, which the flashbang's and the smoke grenade's classes replace) is UE's radial
 * damage with a line of sight (UGameplayStatics::ApplyRadialDamageWithFalloff on the Visibility channel, so a wall
 * shields; pawns do not block it): ExplosionDamage at the centre falling linearly to nothing at ExplosionRadius, the
 * thrower's controller the instigator and the projectile the causer (its ArmorRatio is the victims' armor rule). The
 * thrower is hurt too (CS). A short bright light and ExplodeSound mark it; then the projectile is destroyed.
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

	/**
	 * The throwing weapon's name and kill reward, copied at the throw: the kill feed and the money credit them after
	 * the weapon itself is gone (a thrown grenade leaves the inventory).
	 */
	UPROPERTY()
	FString WeaponName;

	UPROPERTY()
	int32 KillReward = 300;

	/** The explosion's sound (null: silent). */
	UPROPERTY(Transient)
	USoundWave* ExplodeSound = nullptr;

	/**
	 * Throws the projectile: its velocity, the controller credited with the damage and the mesh it shows (UE
	 * ShooterGame: InitVelocity).
	 */
	void Launch(const FVector& Velocity, AController* InInstigatorController, UStaticMesh* Mesh);

	/** Explodes now (the fuse, or a test): Detonate at the explosion's centre, then the projectile goes; once. */
	void Explode();
	[[nodiscard]] bool HasExploded() const
	{
		return bExploded;
	}
	/** The world time the fuse runs out. */
	[[nodiscard]] float GetExplodeTime() const;

	[[nodiscard]] UProjectileMovementComponent* GetMovementComponent() const
	{
		return MovementComp;
	}

	/** Joins the game mode's projectiles (the round's clean-up destroys them). */
	void BeginPlay() override;
	/** Leaves the game mode's projectiles. */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** What the explosion does at Origin (the HE grenade's damage, light and sound). */
	virtual void Detonate(const FVector& Origin);

	/** The thrower's controller (UE ShooterGame: MyController). */
	UPROPERTY(Transient)
	AController* InstigatorController = nullptr;

private:
	/** UE ShooterGame: CollisionComp, the root. */
	UPROPERTY()
	USphereComponent* CollisionComp = nullptr;

	UPROPERTY()
	UStaticMeshComponent* MeshComp = nullptr;

	/** UE ShooterGame: MovementComp. */
	UPROPERTY()
	UProjectileMovementComponent* MovementComp = nullptr;

	/** The fuse (Explode when it runs out). */
	FTimerHandle TimerHandle_Fuse;
	bool bExploded = false;
};

/**
 * Counter-Strike 1.6's flashbang (RadiusFlash): every living player whose eyes the explosion sees (a line on the
 * Visibility channel; walls hide, pawns do not) within FlashRadius is flashed, its thrower and teammates too. The
 * strength falls linearly from FlashStrength at the centre to nothing at FlashRadius; by the angle between the view and
 * the flash (the dot of the view's direction and the direction to the flash) the screen holds white HoldScale x the
 * strength seconds and then fades in FadeScale x the strength: looking at it (dot 0.5 or more) 1.5 and 3 at full
 * white, aside (down to -0.5) 0.45 and 1.75 at 200 of 255, behind 0.2 and 1 at 200 of 255
 * (AShooterCharacter::Flash). A bot is blind (it sees nobody) for BotBlindFadeShare of the fade (CS: a third).
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterProjectile_Flashbang : public AShooterProjectile
{
	GENERATED_BODY()

public:
	AShooterProjectile_Flashbang(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** How far it reaches, cm (CS: 1500 units), and its strength at the centre (CS: 4). */
	UPROPERTY(Config)
	float FlashRadius = 3810.0f;

	UPROPERTY(Config)
	float FlashStrength = 4.0f;

	/** The views' bands (the dot's thresholds) and their hold and fade per point of strength and their white. */
	UPROPERTY(Config)
	float FacingDot = 0.5f;

	UPROPERTY(Config)
	float AsideDot = -0.5f;

	UPROPERTY(Config)
	FVector FacingFlash = FVector(1.5f, 3.0f, 1.0f);

	UPROPERTY(Config)
	FVector AsideFlash = FVector(0.45f, 1.75f, 200.0f / 255.0f);

	UPROPERTY(Config)
	FVector BehindFlash = FVector(0.2f, 1.0f, 200.0f / 255.0f);

	/** The part of the fade a bot is blind (CS: 0.33). */
	UPROPERTY(Config)
	float BotBlindFadeShare = 0.33f;

	/**
	 * The flash a view at EyeLocation looking along ViewDirection gets from an explosion at Origin, walls aside: its
	 * hold and fade (seconds) and how white (0 to 1); false out of range.
	 */
	bool ComputeFlash(const FVector& Origin, const FVector& EyeLocation, const FVector& ViewDirection, float& OutHold,
		float& OutFade, float& OutAlpha) const;

protected:
	/** Flashes the players the explosion sees (see the class comment), with a bright light and the bang. */
	void Detonate(const FVector& Origin) override;
};

/**
 * Counter-Strike 1.6's smoke grenade: where it goes off it leaves an AShooterSmokeCloud (SmokeCloudClass) on the floor
 * below.
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterProjectile_Smoke : public AShooterProjectile
{
	GENERATED_BODY()

public:
	AShooterProjectile_Smoke(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The cloud it leaves. */
	UPROPERTY()
	TSubclassOf<AShooterSmokeCloud> SmokeCloudClass;

protected:
	/** Spawns the cloud on the floor under Origin. */
	void Detonate(const FVector& Origin) override;
};
