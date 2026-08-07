#pragma once

// Prefer CMake `-DLEON_ENGINE_VERSION=\"…\"` (Editor PROJECT_VERSION). Fallback matches last release.
#ifndef LEON_ENGINE_VERSION
#define LEON_ENGINE_VERSION "0.9.0"
#endif

namespace leon {

/// Marketing / hub version string (e.g. Welcome "Engine 0.9.0").
[[nodiscard]] inline constexpr const char* EngineVersionString() {
    return LEON_ENGINE_VERSION;
}

} // namespace leon
