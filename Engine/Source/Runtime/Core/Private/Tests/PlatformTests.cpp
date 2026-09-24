#include "CoreTypes.h"
#include "HAL/PlatformProperties.h"
#include "Modules/ModuleManager.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("Platform types have Unreal sizes", "[Core][HAL]")
{
	STATIC_REQUIRE(sizeof(int8) == 1);
	STATIC_REQUIRE(sizeof(int16) == 2);
	STATIC_REQUIRE(sizeof(int32) == 4);
	STATIC_REQUIRE(sizeof(int64) == 8);
	STATIC_REQUIRE(sizeof(UPTRINT) == sizeof(void*));
}

TEST_CASE("Exactly one platform macro is set", "[Core][HAL]")
{
	REQUIRE(PLATFORM_WINDOWS + PLATFORM_LINUX + PLATFORM_PS2 == 1);
	REQUIRE(PLATFORM_DESKTOP == 1);
	REQUIRE(std::string(FPlatformProperties::PlatformName()).size() > 0);
}

TEST_CASE("Module manager started the Core module", "[Core][Modules]")
{
	FModuleManager& ModuleManager = FModuleManager::Get();
	REQUIRE(ModuleManager.IsModuleLoaded("Core"));
	REQUIRE(ModuleManager.GetModuleCount() >= 1);
	REQUIRE(std::string(ModuleManager.GetModuleName(0)) == "Core");
}
