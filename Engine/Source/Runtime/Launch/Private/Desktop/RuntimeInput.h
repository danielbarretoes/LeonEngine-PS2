#pragma once

#include "Engine/GameEngine.h"


/// Thin wire: ensure default UInputMappingContext is present (Engine ctor already seeds one).
inline void WireDefaultInput(UGameEngine& engine) {
    (void)engine;
    // Intentionally minimal — project packs can AddMappingContext on the shared Engine input.
}

