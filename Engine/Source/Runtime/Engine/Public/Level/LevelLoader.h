#pragma once

#include <cstddef>
#include <leon/Engine.h>
#include <leon/level/LevelAnimation.h>
#include <string>

namespace leon {

struct StaticMeshComponent;

/// Optional: scale mesh to `fitHeight` and ground-align (bottom at y≈0). `position` is a post-fit
/// offset.
void ApplyFitHeight(StaticMeshComponent& object, float fitHeight);

/// Loads a binary Leon Level (`.llev`) into an Engine. Any other extension is rejected — there is
/// no JSON level format. Builds into a staging Level and commits only on success (failed loads
/// leave the previous Level and camera untouched).
/// Runs ContentValidator on the decoded document (referenced materials / meshes) before applying.
bool LoadLevelFile(Engine& engine, const std::string& levelPath, LevelAnimation* outAnim = nullptr);

} // namespace leon
