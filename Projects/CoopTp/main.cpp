#include <leon/packs/CoopTp/RegisterModes.h>
#include <leon/runtime/RunLeonGame.h>

#if !defined(LEON_DEDICATED_DEFAULT)
#define LEON_DEDICATED_DEFAULT 0
#endif

int main(int argc, char** argv) {
    // LEON_DEDICATED_DEFAULT is set on leon-CoopTp-server only (this TU). Passing it into
    // RunLeonGame is required: the define does not apply inside leon_runtime's GameApplication.
    constexpr bool kDedicatedByDefault = (LEON_DEDICATED_DEFAULT != 0);
    // Flow (Unreal-like): GameInstance → MainMenu → Lobby → Match maps.
    return leon::runtime::RunLeonGame(argc, argv, "CoopTp", &leon::packs::coop_tp::RegisterModes,
                                      kDedicatedByDefault);
}
