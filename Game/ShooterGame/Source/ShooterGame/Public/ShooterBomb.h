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
 * - Carried: it goes where its carrier goes, hidden. The carrier drops it when it dies, or with the drop key while it
 *   is drawn (AShooterCharacter::DropBomb, CS's C4 in slot 5).
 * - Dropped: it lies on the floor; the first live terrorist within PickupRadius takes it (the one who dropped it only
 *   after PickupDelay), and its player reads "Picked up C4".
 * - Planted: the carrier plants it by holding the use key, standing still in a bomb site, for PlantDuration
 *   (AShooterCharacter). It beeps, faster as its BombTimer runs out, and explodes: ExplosionDamage falling to nothing
 *   at ExplosionRadius, through walls (CS), armor taking its share (ArmorRatio), and the terrorists win the round.
 * - Defused: a counter-terrorist within DefuseRadius holds the use key for DefuseDuration (DefuseKitDuration with a
 *   kit). Leaving the bomb's reach, letting go or dying stops it; a defuse that finishes before the explosion wins the
 *   round for the counter-terrorists.
 *
 * The game mode hears of each step (OnBombPlanted, OnBombDefused, OnBombExploded) and copies the state to the game
 * state for the HUD. The explosion, the defuse and the beeps are timers of the world's timer manager; a defuse that
 * ends on the explosion's step wins.
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

	/**
	 * The explosion's damage at the centre and its reach, cm (CS 1.6: 500 damage out to 3.5 times that in units, 1750
	 * units = 4445 cm).
	 */
	UPROPERTY(Config)
	float ExplosionDamage = 500.0f;

	UPROPERTY(Config)
	float ExplosionRadius = 4445.0f;

	/**
	 * The victims' armor rule for the blast (AShooterWeapon::ArmorRatio). CS 1.6 lets armor take blast damage like any
	 * other (health takes half, the armor half the rest): 1, the HE grenade's.
	 */
	UPROPERTY(Config)
	float ArmorRatio = 1.0f;

	/** How near a terrorist's feet a dropped bomb is picked up, and a defuser's feet must stay, cm. */
	UPROPERTY(Config)
	float PickupRadius = 60.0f;

	UPROPERTY(Config)
	float DefuseRadius = 120.0f;

	/** Seconds after a drop before the terrorist who dropped it can take it back (a weapon's PickupDelay). */
	UPROPERTY(Config)
	float PickupDelay = 1.0f;

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
	/**
	 * The world time it explodes, set when it is planted (the HUD's count down; the explosion is its timer's), and the
	 * time the defuse ends (defusing; 0 without).
	 */
	[[nodiscard]] float GetExplodeTime() const
	{
		return ExplodeTime;
	}
	[[nodiscard]] float GetDefuseEndTime() const;
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
	/** Joins the game mode's bombs. */
	void BeginPlay() override;
	void Tick(float DeltaSeconds) override;
	/**
	 * Leaving the world (the round's clean-up) frees its carrier (nobody carries a destroyed bomb) and leaves the game
	 * mode's bombs and pickups.
	 */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** A planted bomb's beep, and the next one: every second, then faster over the last 10 seconds (its timer). */
	void Beep();
	/** The bomb's timer ran out: it explodes, unless a defuse ends on the same step. */
	void OnExplodeTimer();
	/** The defuse's timer ran out: the bomb is defused. */
	void OnDefuseTimer();
	/** The world time a timer ends at, 0 without it. */
	[[nodiscard]] float GetTimerEndTime(FTimerHandle Handle) const;
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

	/** Who dropped it last, and the world time from which it may take it back (PickupDelay). */
	UPROPERTY(Transient)
	AShooterCharacter* Dropper = nullptr;

	float DropperPickupTime = 0.0f;

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
	FTimerHandle TimerHandle_Explode;
	FTimerHandle TimerHandle_Defuse;
	FTimerHandle TimerHandle_Beep;
};
