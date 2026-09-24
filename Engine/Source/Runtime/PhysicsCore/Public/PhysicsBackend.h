#pragma once

#include <cstdint>


/// Physics implementation behind `FPhysScene`.
/// Default remains Arcade (AABB traces + CMC). Pass `EPhysicsBackend::Jolt` for rigid Step
/// and narrow-phase traces when built with `LEON_WITH_JOLT` (Editor/Engine default ON).
enum class EPhysicsBackend : std::uint8_t {
    Arcade = 0,
    Jolt = 1,
};

/// Default backend for new FPhysScene / World instances (Arcade — CMC + AABB queries).
[[nodiscard]] inline EPhysicsBackend DefaultPhysicsBackend() {
    return EPhysicsBackend::Arcade;
}

[[nodiscard]] inline const char* PhysicsBackendName(EPhysicsBackend Backend) {
    switch (Backend) {
    case EPhysicsBackend::Arcade:
        return "Arcade";
    case EPhysicsBackend::Jolt:
        return "Jolt";
    }
    return "Unknown";
}

