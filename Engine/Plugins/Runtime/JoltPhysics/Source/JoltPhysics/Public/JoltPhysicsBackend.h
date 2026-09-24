#pragma once

#include "IPhysicsBackend.h"
#include <memory>


/// Factory for the Jolt rigid-body backend (Plugins/Physics/Jolt).
[[nodiscard]] std::unique_ptr<IPhysicsBackend> CreateJoltPhysicsBackend();

