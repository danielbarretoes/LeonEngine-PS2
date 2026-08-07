#pragma once

#include <leon/Engine.h>

namespace leon::runtime {

/// Thin wire: ensure default InputMappingContext is present (Engine ctor already seeds one).
inline void WireDefaultInput(Engine& engine) {
    (void)engine;
    // Intentionally minimal — project packs can AddMappingContext on the shared Engine input.
}

} // namespace leon::runtime
