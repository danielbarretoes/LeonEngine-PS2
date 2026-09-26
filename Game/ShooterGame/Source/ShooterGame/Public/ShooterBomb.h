#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterTypes.h"
#include "UObject/SoftObjectPath.h"
#include "ShooterBomb.generated.h"

class AShooterCharacter;
class USoundWave;
class UStaticMeshComponent;

/**
 * Counter-Strike's C4 (the bomb of a defusal map). One per round: the game mode gives it to a terrorist at the round's
 * start (AShooterGameMode::StartRound).
 *
 * - Carried: it goes where its carrier goes, hidden. The carrier drops it when it dies.
 * - Dropped: it lies on the floor; the first live terrorist within PickupRadius takes it.
 * - Planted: the carrier plants it by holding the use key, standing still in a bomb site, for PlantDuration
 *   (AShooterCharacter). It beeps, faster as its BombTimer runs out, and explodes: ExplosionDamage falling to nothing
 *   at ExplosionRadius, through walls (CS), and the terrorists win the round.
 * - Defused: a counter-terrorist within DefuseRadius holds the use key for DefuseDuration (DefuseKitDuration with a
 *   kit). Leaving the bomb's reach, letting go or dying stops it; a defuse that finishes before the explosion wins the
 *   round for the counter-terrorists.
 *
 * The game mode hears of each step (OnBombPlanted, OnBombDefused, OnBombExploded) and copies the state to the game
 * state for the HUD.
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterBomb : public AActor
{
	GENERATED_BODY()

public:
	AShooterBomb(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Seconds from the plant to the explosion (CS: mp_c4timer 40 s here, the plan's value). */
	UPROPERTY(Config)
	float BombTimer = 40.0f;

	/** Seconds of holding the use key to plant (CS: 3 s). */
	UPROPERTY(Config)
	float PlantDuration = 3.0f;

	/** Seconds to defuse without and with a kit (CS: 10 s and 5 s). */
	UPROPERTY(Config)
	float DefuseDuration = 10.0f;

	UPROPERTY(Config)
	float DefuseKitDuration = 5.0f;

	/** The explosion's damage at the centre and its reach, cm (CS: 500 damage, about 700 units). */
	UPROPERTY(Config)
	float ExplosionDamage = 500.0f;

	UPROPERTY(Config)
	float ExplosionRadius = 1750.0f;

	/** How near a terrorist's feet a dropped bomb is picked up, and a defuser's feet must stay, cm. */
	UPROPERTY(Config)
	float PickupRadius = 60.0f;

	UPROPERTY(Config)
	float DefuseRadius = 120.0f;

	/** The bomb's mesh (/Game/Weapons/SM_C4) and sounds. */
	UPROPERTY(Config)
	FSoftObjectPath MeshName;

	UPROPERTY(Config)
	FSoftObjectPath BeepSoundName;

	UPROPERTY(Config)
	FSoftObjectPath PlantSoundName;

	UPROPERTY(Config)
	FSoftObjectPath DefuseSoundName;

	UPROPERTY(Config)
	FSoftObjectPath ExplodeSoundName;

	[[nodiscard]] EShooterBombState GetBombState() const
	{
		return State;
	}
	/** The terrorist carrying it, or null. */
	[[nodiscard]] AShooterCharacter* GetCarrier() const
	{
		return Carrier;
	}
	/** The counter-terrorist defusing it, or null. */
	[[nodiscard]] AShooterCharacter* GetDefuser() const
	{
		return Defuser;
	}
	/** The world time it explodes (planted), and the time the defuse ends (defusing). */
	[[nodiscard]] float GetExplodeTime() const
	{
		return ExplodeTime;
	}
	[[nodiscard]] float GetDefuseEndTime() const
	{
		return DefuseEndTime;
	}
	/** The site it was planted in ("A", "B"). */
	[[nodiscard]] FName GetSite() const
	{
		return Site;
	}

	/** A terrorist takes it (the round's start, a pickup). */
	void GiveTo(AShooterCharacter* NewCarrier);
	/** Its carrier lets it fall at Location (its feet). */
	void Drop(const FVector& Location);
	/** Planted at Location in Site by Planter: the timer starts. */
	void Plant(const FVector& Location, FName InSite, AShooterCharacter* Planter);
	/** A counter-terrorist starts defusing; false when it cannot (not planted, taken, out of reach). */
	bool StartDefuse(AShooterCharacter* NewDefuser);
	/** Stops a defuse (the use key let go). */
	void StopDefuse(AShooterCharacter* OldDefuser);
	/** Explodes now (the timer, or a test). */
	void Explode();

	void PostInitializeComponents() override;
	void Tick(float DeltaSeconds) override;
	/** Leaving the world (the round's clean-up) frees its carrier: nobody carries a destroyed bomb. */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** The planted bomb's beeps: every second, then faster over the last 10 seconds. */
	void TickBeeps(float Now);
	/** True when a pawn can still defuse: alive, near, holding the key. */
	[[nodiscard]] bool CanKeepDefusing(const AShooterCharacter& Pawn) const;
	void PlaySound(USoundWave* Sound) const;
	[[nodiscard]] float GetWorldTime() const;

	UPROPERTY()
	UStaticMeshComponent* Mesh = nullptr;

	UPROPERTY(Transient)
	AShooterCharacter* Carrier = nullptr;

	UPROPERTY(Transient)
	AShooterCharacter* Defuser = nullptr;

	UPROPERTY(Transient)
	USoundWave* BeepSound = nullptr;

	UPROPERTY(Transient)
	USoundWave* PlantSound = nullptr;

	UPROPERTY(Transient)
	USoundWave* DefuseSound = nullptr;

	UPROPERTY(Transient)
	USoundWave* ExplodeSound = nullptr;

	EShooterBombState State = EShooterBombState::None;
	FName Site;
	float ExplodeTime = 0.0f;
	float DefuseEndTime = 0.0f;
	float NextBeepTime = 0.0f;
};
