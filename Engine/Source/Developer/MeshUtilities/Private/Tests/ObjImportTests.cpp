#include "ObjImport.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("LoadObj imports the Cube fixture", "[MeshUtilities][OBJ]")
{
	const std::filesystem::path Fixture = std::filesystem::path(LEON_ROOT_DIR) /
		"Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/Cube.obj";
	REQUIRE(std::filesystem::exists(Fixture));

	const MeshData Data = LoadObj(Fixture.string());
	REQUIRE_FALSE(Data.empty());
	REQUIRE(Data.indices.size() == 36);
}
