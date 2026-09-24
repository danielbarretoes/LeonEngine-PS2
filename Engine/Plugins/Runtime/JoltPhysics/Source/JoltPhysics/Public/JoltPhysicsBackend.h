#pragma once

#include "IPhysicsBackend.h"
#include <memory>

namespace leon {

/// Factory for the Jolt rigid-body backend (Plugins/Physics/Jolt).
[[nodiscard]] std::unique_ptr<IPhysicsBackend> CreateJoltPhysicsBackend();

} // namespace leon
