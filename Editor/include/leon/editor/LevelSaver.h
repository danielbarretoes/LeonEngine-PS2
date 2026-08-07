#pragma once

#include <leon/core/Camera.h>
#include <leon/Engine.h>
#include <leon/level/Level.h>
#include <string>

namespace leon::editor {

/// Writes a binary Leon Level (`.llev`, version 2) readable by `LoadLevelFile`.
[[nodiscard]] bool saveLevelFile(const std::string& levelPath, const leon::Level& level,
                                 const leon::Camera& camera);

/// Serialize level + camera to `.llev` bytes held in a `std::string` (undo stack / clipboard).
[[nodiscard]] std::string SerializeLevelSnapshot(const leon::Level& level,
                                                 const leon::Camera& camera);

/// Restore a snapshot produced by `SerializeLevelSnapshot` into the engine.
[[nodiscard]] bool LoadLevelSnapshot(leon::Engine& engine, const std::string& bytes,
                                     const std::string& debugName = "memory");

} // namespace leon::editor
