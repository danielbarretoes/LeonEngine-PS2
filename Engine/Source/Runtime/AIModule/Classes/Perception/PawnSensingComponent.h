#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "PawnSensingComponent.generated.h"

class AActor;
class APawn;
class UWorld;

/** A pawn was seen (UE: FSeePawnDelegate). */
using FSeePawnDelegate = TMulticastDelegate<void(APawn* /*Pawn*/)>;
/** A noise was heard (UE: FHearNoiseDelegate): who made it, where, how loud. */
using FHearNoiseDelegate =
	TMulticastDelegate<void(APawn* /*Instigator*/, const FVector& /*Location*/, float /*Volume*/)>;

/**
 * A pawn's senses (UE: UPawnSensingComponent), for the AI controllers: sight and hearing.
 *
 * - Sight: every SensingInterval seconds, each pawn of the world that ShouldCheckVisibilityOf lets through (a player's
 *   only with bOnlySensePlayers), within SightRadius, inside the peripheral vision cone around the owner's view
 *   rotation, and in line of sight (a line on the Visibility channel from the owner's eyes to the pawn's eyes or its
 *   centre meets no wall; pawns do not block it) is broadcast with OnSeePawn. SetTimer moves the next update (a game
 *   spreads its sensors over the interval, so they do not all trace in the same frame).
 * - Hearing: AActor::MakeNoise reaches the registered sensing components of the world (the module sets the noise
 *   delegate; a component lists itself while it is registered, so a noise does not walk the actors): a noise that
 *   ShouldCheckAudibilityOf lets through, within HearingThreshold x Loudness, or within LOSHearingThreshold x Loudness
 *   in line of sight, is broadcast with OnHearNoise (not the owner's own noises).
 *
 * The sight updates run from the world's timer manager (UE: SetTimer / OnTimer): once the component begins play, at the
 * next step, then every SensingInterval. PawnSensing is the UE 4 component the plan names, not the AI Perception
 * system.
 */
UCLASS()
class AIMODULE_API UPawnSensingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPawnSensingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Hears noises within this, cm, through walls (UE: HearingThreshold). */
	UPROPERTY()
	float HearingThreshold = 1400.0f;

	/** Hears noises within this, cm, in line of sight (UE: LOSHearingThreshold). */
	UPROPERTY()
	float LOSHearingThreshold = 2800.0f;

	/** Sees pawns within this, cm (UE: SightRadius). */
	UPROPERTY()
	float SightRadius = 5000.0f;

	/** Seconds between sight updates (UE: SensingInterval). */
	UPROPERTY()
	float SensingInterval = 0.5f;

	/** Senses anything at all (UE: bEnableSensingUpdates). */
	UPROPERTY()
	bool bEnableSensingUpdates = true;

	/** Only player pawns are seen (UE: bOnlySensePlayers). */
	UPROPERTY()
	bool bOnlySensePlayers = true;

	/** Sight and hearing on (UE: bSeePawns, bHearNoises). */
	UPROPERTY()
	bool bSeePawns = true;

	UPROPERTY()
	bool bHearNoises = true;

	FSeePawnDelegate OnSeePawn;
	FHearNoiseDelegate OnHearNoise;

	/** The half angle of the vision cone, degrees (UE: SetPeripheralVisionAngle / GetPeripheralVisionAngle). */
	void SetPeripheralVisionAngle(float NewPeripheralVisionAngle);
	[[nodiscard]] float GetPeripheralVisionAngle() const
	{
		return PeripheralVisionAngle;
	}
	[[nodiscard]] float GetPeripheralVisionCosine() const
	{
		return PeripheralVisionCosine;
	}

	/** Where the senses are: the owner pawn's eyes and view rotation, else the owner's (UE: GetSensorLocation). */
	[[nodiscard]] FVector GetSensorLocation() const;
	[[nodiscard]] FRotator GetSensorRotation() const;

	/** Whether Other can be seen now: range, cone and line of sight (UE: CouldSeePawn). */
	[[nodiscard]] bool CouldSeePawn(const APawn* Other, bool bMaySkipChecks = false) const;
	/**
	 * A line on the Visibility channel from the sensor to Other's eyes or centre meets nothing (UE: HasLineOfSightTo,
	 * virtual: a game adds what else hides, a smoke grenade's cloud in ShooterGame).
	 */
	[[nodiscard]] virtual bool HasLineOfSightTo(const AActor* Other) const;

	/** Looks at every pawn now and broadcasts OnSeePawn for those seen (UE: UpdateAISensing, sight part). */
	void UpdateAISensing();

	/**
	 * The next sight update comes TimeInterval seconds from now (0: the next step), then one every SensingInterval
	 * (UE: SetTimer); it needs the component's world.
	 */
	void SetTimer(float TimeInterval);

	/** How many sight updates the component has run (UpdateAISensing), and the line of sight traces they made. */
	[[nodiscard]] int32 GetNumSightUpdates() const
	{
		return NumSightUpdates;
	}
	[[nodiscard]] int32 GetNumSightTraces() const
	{
		return NumSightTraces;
	}

	/** A noise at Location of Loudness by Instigator: broadcasts OnHearNoise when heard (see the class comment). */
	void HandleNoise(APawn* Instigator, const FVector& Location, float Loudness);

	/**
	 * The registered sensing components of World hear a noise (the module's noise delegate, AActor::MakeNoise): only
	 * the listeners are visited, not the world's actors.
	 */
	static void BroadcastNoise(UWorld& World, APawn* Instigator, const FVector& Location, float Loudness);

	/** Starts the sight updates (UE: the component's BeginPlay sets its timer). */
	void BeginPlay() override;

protected:
	/**
	 * Whether Pawn is looked at, before its range, cone and line of sight (UE: ShouldCheckVisibilityOf): not without
	 * bSeePawns, nor another than a player's with bOnlySensePlayers. A game overrides it to skip the pawns its AI
	 * ignores anyway (teammates, the dead), which saves their traces.
	 */
	[[nodiscard]] virtual bool ShouldCheckVisibilityOf(const APawn* Pawn) const;

	/**
	 * Whether the noises of NoiseInstigator are listened to, before their distance and line of sight (UE:
	 * ShouldCheckAudibilityOf): not without bHearNoises. A game overrides it as ShouldCheckVisibilityOf.
	 */
	[[nodiscard]] virtual bool ShouldCheckAudibilityOf(const APawn* NoiseInstigator) const;

	/**
	 * The sight update, when the timer is due (UE: OnTimer): UpdateAISensing. A game overrides it to hold a look back
	 * to the next step (RetryOnNextTick), e.g. to cap the looks of a step; the timer keeps its phase meanwhile.
	 */
	virtual void OnTimer();

	/**
	 * OnTimer runs again at the next step, besides the SensingInterval timer (Leon): the component's tick, on only
	 * while a look waits (nothing allocated).
	 */
	void RetryOnNextTick();

	/** A look held back (RetryOnNextTick) runs, and the tick goes off again. */
	void TickComponent(float DeltaTime) override;

	/** A registered component is a listener BroadcastNoise visits (in the order they registered). */
	void OnRegister() override;
	void OnUnregister() override;

private:
	UPROPERTY()
	float PeripheralVisionAngle = 90.0f;

	float PeripheralVisionCosine = 0.0f;
	/** The SensingInterval timer and a look held back to the next step (UE: TimerHandle_OnTimer). */
	FTimerHandle TimerHandle_OnTimer;
	int32 NumSightUpdates = 0;
	mutable int32 NumSightTraces = 0;
};
