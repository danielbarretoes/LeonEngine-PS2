#pragma once

#include "Engine/GameEngine.h"
#include "Level/LevelAnimation.h"

#include <cstddef>
#include <string>

struct UStaticMeshComponent;

/// Optional: scale mesh to `fitHeight` and ground-align (bottom at y≈0). `position` is a post-fit
/// offset.
void ApplyFitHeight(UStaticMeshComponent& Object, float FitHeight);

/// Loads a binary Leon Level (`.llev`) into an Engine. Any other extension is rejected — there is
/// no JSON level format. Builds into a staging Level and commits only on success (failed loads
/// leave the previous Level and camera untouched).
/// Runs ContentValidator on the decoded document (referenced materials / meshes) before applying.
bool LoadLevelFile(UGameEngine& Engine, const std::string& LevelPath, FLevelAnimation* OutAnim = nullptr);
