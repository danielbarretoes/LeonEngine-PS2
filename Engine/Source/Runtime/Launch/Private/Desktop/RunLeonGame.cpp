#include "RunLeonGame.h"

#include "GameApplication.h"


int RunLeonGame(int argc, char** argv, const char* packName,
                const std::function<void(UGameEngine&, FGameplayRouter&)>& registerModes,
                bool dedicatedByDefault) {
    return FGameApplication{}.Run(argc, argv, packName, registerModes, dedicatedByDefault);
}

