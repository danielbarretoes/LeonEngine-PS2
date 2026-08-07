#include <leon/editor/PiePackRegistry.h>

#include <leon/core/Ascii.h>

namespace leon::editor {

PiePackInfo FindPiePack(std::string_view projectName) {
    const std::string key = AsciiToLower(std::string(projectName));
    PiePackInfo info;
    // Host sample packs were removed; Blank / template seeds use DefaultGameMode only.
    if (key == "blank" || key == "ps2lab" || key == "thirdperson") {
        info.known = true;
        info.registerModes = {};
        return info;
    }
    return info;
}

} // namespace leon::editor
