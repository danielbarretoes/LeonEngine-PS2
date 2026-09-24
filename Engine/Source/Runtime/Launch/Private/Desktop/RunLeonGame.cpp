#include "RunLeonGame.h"

#include "GameApplication.h"


int RunLeonGame(int argc, char** argv, const char* packName,
                const std::function<void(Engine&, GameplayRouter&)>& registerModes,
                bool dedicatedByDefault) {
    return GameApplication{}.Run(argc, argv, packName, registerModes, dedicatedByDefault);
}

