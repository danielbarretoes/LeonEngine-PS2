#pragma once

#include "CoreMinimal.h"
#include "Engine/GameEngine.h"

struct FLevelStaticMesh;

/**
 * Optional: scale the mesh to FitHeight (its Z extent) and ground-align it (bottom at z ~ 0, centred in X / Y); the
 * existing location is a post-fit offset.
 */
void ApplyFitHeight(FLevelStaticMesh& Object, float FitHeight);

/**
 * Loads a binary Leon Level (.llev) into an Engine. Any other extension is rejected — there is
 * no JSON level format. Builds into a staging Level and commits only on success (failed loads
 * leave the previous Level and camera untouched).
 */
bool LoadLevelFile(UGameEngine& Engine, const FString& LevelPath);
