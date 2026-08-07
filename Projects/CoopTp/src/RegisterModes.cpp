#include <leon/packs/CoopTp/RegisterModes.h>

#include "CoopGameInstance.h"
#include "CoopLobbyGameMode.h"
#include "CoopMenuGameMode.h"
#include "CoopTpGameMode.h"

#include <memory>

namespace leon::packs::coop_tp {

void RegisterModes(Engine& engine, GameplayRouter& router) {
    engine.SetGameInstance<game::CoopGameInstance>();
    router.AddMode(std::make_unique<game::CoopMenuGameMode>());
    router.AddMode(std::make_unique<game::CoopLobbyGameMode>());
    router.AddMode(std::make_unique<game::CoopTpGameMode>());
}

} // namespace leon::packs::coop_tp
