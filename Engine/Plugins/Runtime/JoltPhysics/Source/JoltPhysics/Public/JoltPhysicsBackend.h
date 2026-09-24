#pragma once

#include "IPhysicsBackend.h"

#include <memory>

/// Factory for the Jolt rigid-body backend (JoltPhysics plugin).
[[nodiscard]] std::unique_ptr<IPhysicsBackend> CreateJoltPhysicsBackend();
