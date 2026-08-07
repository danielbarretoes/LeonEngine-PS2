#pragma once

#include <leon/Engine.h>
#include <leon/gameplay/GameplayRouter.h>

namespace leon::packs::coop_tp {

/// Register CoopTp GameInstance + Menu/Lobby/Match GameModes (shipping main + Editor PIE).
void RegisterModes(Engine& engine, GameplayRouter& router);

} // namespace leon::packs::coop_tp
