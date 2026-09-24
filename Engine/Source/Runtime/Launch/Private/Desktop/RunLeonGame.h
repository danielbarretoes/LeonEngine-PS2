#pragma once

#include <functional>
#include "Engine/GameEngine.h"
#include "GameFramework/GameplayRouter.h"


/// Thin runtime host: FLevelDirector + one project's GameMode registration.
/// `packName` is the folder under `Projects/` (e.g. "Smoke").
/// `registerModes(engine, router)` may install a pack UGameInstance via `engine.SetGameInstance<T>()`.
/// `dedicatedByDefault` must come from the pack exe (not leon_runtime): compile defines on the
/// static lib do not reach FGameApplication.cpp.
[[nodiscard]] int RunLeonGame(int argc, char** argv, const char* packName,
                              const std::function<void(UGameEngine&, FGameplayRouter&)>& registerModes,
                              bool dedicatedByDefault = false);

