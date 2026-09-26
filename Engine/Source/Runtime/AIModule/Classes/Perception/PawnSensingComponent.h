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
 * - Sight: every SensingInterval seconds, each pawn of the world (a player's only with bOnlySensePlayers) that is
 *   within SightRadius, inside the peripheral vision cone around the owner's view rotation, and in line of sight
 *   (a line on the Visibility channel from the owner's eyes to the pawn's eyes or its centre meets no wall; pawns do
 *   not block it) is broadcast with OnSeePawn.
 * - Hearing: AActor::MakeNoise reaches every sensing component of the world at once (the module sets the noise
 *   delegate): a noise within HearingThreshold x Loudness, or within LOSHearingThreshold x Loudness in line of sight,
 *   is broadcast with OnHearNoise (not the owner's own noises).
 *
 * Leon: the component ticks itself (UE: the pawn's controller asks it through the sensing update); PawnSensing is the
 * UE 4 component the plan names, not the AI Perception system.
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
	/** A line on the Visibility channel from the sensor to Other's eyes or centre meets nothing (UE: HasLineOfSightTo).
	 */
	[[nodiscard]] bool HasLineOfSightTo(const AActor* Other) const;

	/** Looks at every pawn now and broadcasts OnSeePawn for those seen (UE: UpdateAISensing, sight part). */
	void UpdateAISensing();

	/** A noise at Location of Loudness by Instigator: broadcasts OnHearNoise when heard (see the class comment). */
	void HandleNoise(APawn* Instigator, const FVector& Location, float Loudness);

	/** The world's sensing components hear a noise (the module's noise delegate, AActor::MakeNoise). */
	static void BroadcastNoise(UWorld& World, APawn* Instigator, const FVector& Location, float Loudness);

	void TickComponent(float DeltaTime) override;

private:
	UPROPERTY()
	float PeripheralVisionAngle = 90.0f;

	float PeripheralVisionCosine = 0.0f;
	float TimeUntilNextUpdate = 0.0f;
};
