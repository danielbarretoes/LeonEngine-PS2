#include <leon/packs/Zombies/RegisterModes.h>
#include <leon/runtime/RunLeonGame.h>

#if !defined(LEON_DEDICATED_DEFAULT)
#define LEON_DEDICATED_DEFAULT 0
#endif

int main(int argc, char** argv) {
    constexpr bool kDedicatedByDefault = (LEON_DEDICATED_DEFAULT != 0);
    // Flow: GameInstance -> MainMenu -> Lobby -> Town match.
    return leon::runtime::RunLeonGame(argc, argv, "Zombies", &leon::packs::zombies::RegisterModes,
                                      kDedicatedByDefault);
}
