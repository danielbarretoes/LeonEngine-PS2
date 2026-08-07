#include <leon/packs/Furytoon/RegisterModes.h>
#include <leon/runtime/RunLeonGame.h>

#if !defined(LEON_DEDICATED_DEFAULT)
#define LEON_DEDICATED_DEFAULT 0
#endif

int main(int argc, char** argv) {
    constexpr bool kDedicatedByDefault = (LEON_DEDICATED_DEFAULT != 0);
    // Flow: GameInstance -> MainMenu -> Lobby -> Kitchen match.
    return leon::runtime::RunLeonGame(argc, argv, "Furytoon", &leon::packs::furytoon::RegisterModes,
                                      kDedicatedByDefault);
}
