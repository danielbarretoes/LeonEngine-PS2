#pragma once

#include "ZombiesCharacter.h"

namespace game {

/// AI-controlled zombie pawn. Thin alias for clarity in ZombiesGameMode while reusing the same
/// capsule / movement / Health / TakeDamage plumbing (no skeletal mesh).
using ZombieCharacter = ZombiesCharacter;

} // namespace game
