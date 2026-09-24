#pragma once

#include <functional>
#include "Engine/GameEngine.h"
#include "GameFramework/GameplayRouter.h"


/// Game process shell: init Engine, load project pack, tick FWorldRuntime, run loop.
class FGameApplication {
public:
    /// When `dedicatedByDefault` is true (server shipping exes), start headless without CLI flags.
    /// CLI `--dedicated` / `--server` still force dedicated on client builds.
    [[nodiscard]] int Run(int Argc, char** Argv, const char* PackName,
                          const std::function<void(UGameEngine&, FGameplayRouter&)>& RegisterModes,
                          bool bDedicatedByDefault = false);
};

