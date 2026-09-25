#pragma once

#include "CoreMinimal.h"
#include "Engine/GameEngine.h"

class UStaticMesh;

/**
 * Optional: scale Transform so Mesh is FitHeight tall (its Z extent) and ground-align it (bottom at z ~ 0, centred in
 * X / Y); the existing location is a post-fit offset.
 */
void ApplyFitHeight(FTransform& Transform, const UStaticMesh& Mesh, float FitHeight);

/**
 * Loads a binary Leon Level (.llev) into an Engine's world as actors. Any other extension is rejected — there is
 * no JSON level format. Resources are resolved before anything is spawned: a failed load leaves the previous level
 * actors and the camera untouched.
 */
bool LoadLevelFile(UGameEngine& Engine, const FString& LevelPath);
