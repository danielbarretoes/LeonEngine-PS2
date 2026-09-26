#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"

class ACharacter;
class APainCausingVolume;
class ATriggerVolume;

/** True when the character's feet lie inside the volume's brush box (AVolume::EncompassesPoint). */
[[nodiscard]] bool CharacterOverlapsPainVolume(const ACharacter& Ch, const APainCausingVolume& Vol);

/**
 * Apply one pain tick: DamagePerSec * PainInterval via UGameplayStatics::ApplyDamage (the volume's DamageType, the
 * volume the causer).
 */
void ApplyPainVolumeDamage(ACharacter& Ch, const APainCausingVolume& Vol);

/**
 * Authority tick: global accumulator (Zombies lava style). When TickAccum reaches the smallest positive PainInterval
 * among the volumes that cause pain, damages each character that can be damaged and overlaps any of them (one tick
 * from the first overlapping volume).
 */
void TickPainCausingVolumes(TArrayView<APainCausingVolume* const> Volumes, TArrayView<ACharacter*> Characters,
	float DeltaTime, float& TickAccum);

/**
 * Nearest trigger volume whose XY distance from Feet is within min(MaxDist, its interact radius), or null. The
 * interact radius comes from the volume's UInteractableComponent (200 cm without one).
 */
[[nodiscard]] ATriggerVolume* FindBestTriggerVolume(
	TArrayView<ATriggerVolume* const> Volumes, const FVector& Feet, float MaxDist);

/** Default [F] … [cost] prompt from the volume's UInteractableComponent payload / cost (Door, WallBuy:…, Perk:…). */
[[nodiscard]] FString FormatDefaultInteractPrompt(const ATriggerVolume& Volume);
