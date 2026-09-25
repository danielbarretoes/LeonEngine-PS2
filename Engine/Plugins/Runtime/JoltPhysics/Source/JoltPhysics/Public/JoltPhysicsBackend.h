#pragma once

#include "IPhysicsBackend.h"

/** Factory for the Jolt rigid-body backend (JoltPhysics plugin). */
[[nodiscard]] TUniquePtr<IPhysicsBackend> CreateJoltPhysicsBackend();
