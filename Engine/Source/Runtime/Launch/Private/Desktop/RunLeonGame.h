#pragma once

#include <functional>
#include "Engine/GameEngine.h"
#include "GameFramework/GameplayRouter.h"


/// Thin runtime host: LevelDirector + one project's GameMode registration.
/// `packName` is the folder under `Projects/` (e.g. "Smoke").
/// `registerModes(engine, router)` may install a pack GameInstance via `engine.SetGameInstance<T>()`.
/// `dedicatedByDefault` must come from the pack exe (not leon_runtime): compile defines on the
/// static lib do not reach GameApplication.cpp.
[[nodiscard]] int RunLeonGame(int argc, char** argv, const char* packName,
                              const std::function<void(Engine&, GameplayRouter&)>& registerModes,
                              bool dedicatedByDefault = false);

