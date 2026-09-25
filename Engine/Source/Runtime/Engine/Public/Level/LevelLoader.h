#pragma once

#include "CoreMinimal.h"

class FResourceCache;
class UStaticMesh;
class UWorld;

/**
 * Optional: scale Transform so Mesh is FitHeight tall (its Z extent) and ground-align it (bottom at z ~ 0, centred in
 * X / Y); the existing location is a post-fit offset.
 */
void ApplyFitHeight(FTransform& Transform, const UStaticMesh& Mesh, float FitHeight);

/**
 * Loads a binary Leon Level (.llev) into a world as actors, its meshes and materials through Resources (the engine's
 * FResourceCache). Any other extension is rejected — there is no JSON level format. Resources are resolved before
 * anything is spawned: a failed load leaves the previous level actors untouched. UEngine::LoadMap calls it for a
 * `.llev` map (until the `.lmap` packages of P15).
 */
bool LoadLevelFile(UWorld& World, FResourceCache& Resources, const FString& LevelPath);
