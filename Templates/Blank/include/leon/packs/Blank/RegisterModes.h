#pragma once

#include <leon/Engine.h>
#include <leon/gameplay/GameplayRouter.h>

namespace leon::packs::blank {

/// Blank packs register no custom GameModes (DefaultGameMode only).
void RegisterModes(Engine& engine, GameplayRouter& router);

} // namespace leon::packs::blank
