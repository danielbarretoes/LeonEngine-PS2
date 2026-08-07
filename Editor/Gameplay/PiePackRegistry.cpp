#include <leon/editor/PiePackRegistry.h>

#include <leon/core/Ascii.h>
#include <leon/packs/CoopTp/RegisterModes.h>
#include <leon/packs/Furytoon/RegisterModes.h>
#include <leon/packs/ThirdPerson/RegisterModes.h>
#include <leon/packs/Zombies/RegisterModes.h>

namespace leon::editor {

PiePackInfo FindPiePack(std::string_view projectName) {
    const std::string key = AsciiToLower(std::string(projectName));
    PiePackInfo info;
    if (key == "cooptp") {
        info.known = true;
        info.registerModes = &leon::packs::coop_tp::RegisterModes;
        return info;
    }
    if (key == "zombies") {
        info.known = true;
        info.registerModes = &leon::packs::zombies::RegisterModes;
        return info;
    }
    if (key == "furytoon") {
        info.known = true;
        info.registerModes = &leon::packs::furytoon::RegisterModes;
        return info;
    }
    if (key == "thirdperson") {
        info.known = true;
        info.registerModes = &leon::packs::third_person::RegisterModes;
        return info;
    }
    if (key == "smoke" || key == "blank") {
        // No custom GameModes — GameHostSession uses DefaultGameMode only.
        info.known = true;
        info.registerModes = {};
        return info;
    }
    return info;
}

} // namespace leon::editor
