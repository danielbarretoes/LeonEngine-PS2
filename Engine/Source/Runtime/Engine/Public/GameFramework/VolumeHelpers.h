#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Engine/Level.h"

class ACharacter;

/**
 * True when character feet lie inside the volume AABB
 * (center = transform.position, half-extents = abs(scale) * 0.5).
 */
[[nodiscard]] bool CharacterOverlapsPainVolume(const ACharacter& Ch, const FPainCausingVolume& Vol);

/** Apply one pain tick: damagePerSecond * damageInterval via UGameplayStatics::ApplyPointDamage. */
void ApplyPainVolumeDamage(ACharacter& Ch, const FPainCausingVolume& Vol);

/**
 * Authority tick: global accumulator (Zombies lava style). When tickAccum reaches the
 * smallest positive damageInterval among volumes, damages each alive character that
 * overlaps any volume (one tick from the first overlapping volume).
 */
void TickPainCausingVolumes(
	const TArray<FPainCausingVolume>& Volumes, TArrayView<ACharacter*> Characters, float DeltaTime, float& TickAccum);

/**
 * Nearest FTriggerVolume whose XY distance from feet is within min(maxDist, interactRadius).
 * Returns Level::npos if none.
 */
[[nodiscard]] SIZE_T FindBestTriggerVolume(const TArray<FTriggerVolume>& Volumes, const FVector& Feet, float MaxDist);

/** Default [F] … [cost] prompt from payload / interactCost (Door, WallBuy:…, Perk:…). */
[[nodiscard]] FString FormatDefaultInteractPrompt(const FTriggerVolume& Volume);
