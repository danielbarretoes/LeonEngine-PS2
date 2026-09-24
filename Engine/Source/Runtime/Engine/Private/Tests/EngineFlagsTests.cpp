#include <catch2/catch_test_macros.hpp>
#include "Engine/GameEngine.h"

TEST_CASE("Engine flags work before initialize", "[engine]") {
    Engine engine;
    REQUIRE_FALSE(engine.IsInitialized());

    engine.SetSuppressCameraDrag(true);
    REQUIRE(engine.IsCameraDragSuppressed());
    engine.SetSuppressCameraDrag(false);
    REQUIRE_FALSE(engine.IsCameraDragSuppressed());

    REQUIRE_FALSE(engine.IsCollisionDebugEnabled());
    engine.ToggleCollisionDebug();
    REQUIRE(engine.IsCollisionDebugEnabled());
    engine.SetCollisionDebugEnabled(false);
    REQUIRE_FALSE(engine.IsCollisionDebugEnabled());

    REQUIRE_FALSE(engine.IsNavMeshDebugEnabled());
    engine.ToggleNavMeshDebug();
    REQUIRE(engine.IsNavMeshDebugEnabled());
    engine.SetNavMeshDebugEnabled(false);
    REQUIRE_FALSE(engine.IsNavMeshDebugEnabled());

    engine.SetKeyboardOrbitEnabled(false);
    engine.SetOrbitMouseEnabled(false);

    REQUIRE(engine.GetGameInstance().LevelsOpened() == 0);
    engine.GetGameInstance().NotifyLevelOpened();
    REQUIRE(engine.GetGameInstance().LevelsOpened() == 1);
}
