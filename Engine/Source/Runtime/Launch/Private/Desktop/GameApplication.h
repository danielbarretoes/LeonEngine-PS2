#pragma once

#include <functional>
#include "Engine/GameEngine.h"
#include "GameFramework/GameplayRouter.h"

namespace leon::runtime {

/// Game process shell: init Engine, load project pack, tick WorldRuntime, run loop.
class GameApplication {
public:
    /// When `dedicatedByDefault` is true (server shipping exes), start headless without CLI flags.
    /// CLI `--dedicated` / `--server` still force dedicated on client builds.
    [[nodiscard]] int Run(int argc, char** argv, const char* packName,
                          const std::function<void(Engine&, GameplayRouter&)>& registerModes,
                          bool dedicatedByDefault = false);
};

} // namespace leon::runtime
