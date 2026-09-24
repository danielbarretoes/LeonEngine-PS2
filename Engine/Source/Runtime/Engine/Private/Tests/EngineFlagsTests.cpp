#include "Engine/GameEngine.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Engine flags work before initialize", "[engine]")
{
	UGameEngine Engine;
	REQUIRE_FALSE(Engine.IsInitialized());

	Engine.SetSuppressCameraDrag(true);
	REQUIRE(Engine.IsCameraDragSuppressed());
	Engine.SetSuppressCameraDrag(false);
	REQUIRE_FALSE(Engine.IsCameraDragSuppressed());

	REQUIRE_FALSE(Engine.IsCollisionDebugEnabled());
	Engine.ToggleCollisionDebug();
	REQUIRE(Engine.IsCollisionDebugEnabled());
	Engine.SetCollisionDebugEnabled(false);
	REQUIRE_FALSE(Engine.IsCollisionDebugEnabled());

	REQUIRE_FALSE(Engine.IsNavMeshDebugEnabled());
	Engine.ToggleNavMeshDebug();
	REQUIRE(Engine.IsNavMeshDebugEnabled());
	Engine.SetNavMeshDebugEnabled(false);
	REQUIRE_FALSE(Engine.IsNavMeshDebugEnabled());

	Engine.SetKeyboardOrbitEnabled(false);
	Engine.SetOrbitMouseEnabled(false);

	REQUIRE(Engine.GetGameInstance().GetLevelsOpened() == 0);
	Engine.GetGameInstance().NotifyLevelOpened();
	REQUIRE(Engine.GetGameInstance().GetLevelsOpened() == 1);
}
