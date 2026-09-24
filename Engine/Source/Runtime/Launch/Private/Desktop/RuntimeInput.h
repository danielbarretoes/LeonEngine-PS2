#pragma once

#include "Engine/GameEngine.h"

/// Thin wire: ensure default UInputMappingContext is present (Engine ctor already seeds one).
inline void WireDefaultInput(UGameEngine& Engine)
{
	(void)Engine;
	// Intentionally minimal — games can AddMappingContext on the shared Engine input.
}
