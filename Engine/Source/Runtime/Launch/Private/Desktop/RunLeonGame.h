#pragma once

#include <functional>
#include "Engine/GameEngine.h"
#include "GameFramework/GameplayRouter.h"


/// Thin runtime host: FLevelDirector + one project's GameMode registration.
/// `packName` is the folder under `Projects/` (e.g. "Smoke").
/// `registerModes(engine, router)` may install a pack UGameInstance via `engine.SetGameInstance<T>()`.
/// `dedicatedByDefault` must come from the pack exe (not leon_runtime): compile defines on the
/// static lib do not reach FGameApplication.cpp.
[[nodiscard]] int RunLeonGame(int Argc, char** Argv, const char* PackName,
                              const std::function<void(UGameEngine&, FGameplayRouter&)>& RegisterModes,
                              bool bDedicatedByDefault = false);

