#include "ObjImport.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

TEST_CASE("LoadObj imports the Cube fixture", "[MeshUtilities][OBJ]")
{
	const std::filesystem::path Fixture =
		std::filesystem::path(LEON_ROOT_DIR) / "Engine/Source/Developer/MeshUtilities/Private/Tests/Fixtures/Cube.obj";
	REQUIRE(std::filesystem::exists(Fixture));

	const FMeshData Data = LoadObj(Fixture.string());
	REQUIRE_FALSE(Data.IsEmpty());
	REQUIRE(Data.Indices.Num() == 36);
}

TEST_CASE("LoadObj loads a minimal OBJ", "[MeshUtilities][OBJ]")
{
	const auto Path = std::filesystem::temp_directory_path() / "leon_test_tri.obj";
	{
		std::ofstream Out(Path);
		REQUIRE(Out);
		Out << "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
			<< "vn 0 0 1\n"
			<< "f 1//1 2//1 3//1\n";
	}
	const FMeshData Data = LoadObj(Path.string());
	REQUIRE_FALSE(Data.IsEmpty());
	REQUIRE(Data.Indices.Num() % 3 == 0);
	std::filesystem::remove(Path);
}
