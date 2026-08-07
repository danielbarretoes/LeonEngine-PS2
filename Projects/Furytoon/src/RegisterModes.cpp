#include <leon/packs/Furytoon/RegisterModes.h>

#include "FurytoonGameInstance.h"
#include "FurytoonGameMode.h"
#include "FurytoonLobbyGameMode.h"
#include "FurytoonMenuGameMode.h"

#include <memory>

namespace leon::packs::furytoon {

void RegisterModes(Engine& engine, GameplayRouter& router) {
    engine.SetGameInstance<game::FurytoonGameInstance>();
    router.AddMode(std::make_unique<game::FurytoonMenuGameMode>());
    router.AddMode(std::make_unique<game::FurytoonLobbyGameMode>());
    router.AddMode(std::make_unique<game::FurytoonGameMode>());
}

} // namespace leon::packs::furytoon
