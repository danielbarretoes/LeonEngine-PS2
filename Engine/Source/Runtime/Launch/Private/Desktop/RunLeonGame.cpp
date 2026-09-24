#include "RunLeonGame.h"

#include "GameApplication.h"


int RunLeonGame(int Argc, char** Argv, const char* PackName,
                const std::function<void(UGameEngine&, FGameplayRouter&)>& RegisterModes,
                bool bDedicatedByDefault) {
    return FGameApplication{}.Run(Argc, Argv, PackName, RegisterModes, bDedicatedByDefault);
}

