#include "ObjImport.h"
#include "Primitives.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/geometric.hpp>

#include <filesystem>
#include <fstream>

using Catch::Matchers::WithinAbs;

TEST_CASE("LoadObj loads a minimal OBJ", "[render][meshdata]")
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
	REQUIRE_FALSE(Data.empty());
	REQUIRE(Data.Indices.size() % 3 == 0);
	std::filesystem::remove(Path);
}

TEST_CASE("ComputeTangents produces unit tangents", "[render][meshdata]")
{
	FMeshData Data = MakeCube();
	// MakeCube may already have tangents; recompute from UVs.
	for (auto& V : Data.Vertices)
	{
		V.Tangent = {0.0f, 0.0f, 0.0f, 1.0f};
	}
	ComputeTangents(Data);
	for (const auto& V : Data.Vertices)
	{
		const float Len = glm::length(glm::vec3{V.Tangent});
		REQUIRE_THAT(Len, WithinAbs(1.0f, 1.0e-3f));
		REQUIRE((V.Tangent.w == 1.0f || V.Tangent.w == -1.0f));
	}
}
