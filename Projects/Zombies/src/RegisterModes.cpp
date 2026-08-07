#include <leon/packs/Zombies/RegisterModes.h>

#include "ZombiesGameInstance.h"
#include "ZombiesGameMode.h"
#include "ZombiesLobbyGameMode.h"
#include "ZombiesMenuGameMode.h"

#include <memory>

namespace leon::packs::zombies {

void RegisterModes(Engine& engine, GameplayRouter& router) {
    engine.SetGameInstance<game::ZombiesGameInstance>();
    router.AddMode(std::make_unique<game::ZombiesMenuGameMode>());
    router.AddMode(std::make_unique<game::ZombiesLobbyGameMode>());
    router.AddMode(std::make_unique<game::ZombiesGameMode>());
}

} // namespace leon::packs::zombies
