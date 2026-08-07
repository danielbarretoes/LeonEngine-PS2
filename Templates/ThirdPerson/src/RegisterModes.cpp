#include <leon/packs/ThirdPerson/RegisterModes.h>

#include "ThirdPersonGameMode.h"

#include <memory>

namespace leon::packs::third_person {

void RegisterModes(Engine& /*engine*/, GameplayRouter& router) {
    router.AddMode(std::make_unique<game::ThirdPersonGameMode>());
}

} // namespace leon::packs::third_person
