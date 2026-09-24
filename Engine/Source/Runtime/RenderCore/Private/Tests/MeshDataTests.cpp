#include <glm/geometric.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <filesystem>
#include <fstream>
#include "ObjImport.h"
#include "Primitives.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("LoadObj loads a minimal OBJ", "[render][meshdata]") {
    const auto path = std::filesystem::temp_directory_path() / "leon_test_tri.obj";
    {
        std::ofstream out(path);
        REQUIRE(out);
        out << "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
            << "vn 0 0 1\n"
            << "f 1//1 2//1 3//1\n";
    }
    const leon::MeshData data = leon::LoadObj(path.string());
    REQUIRE_FALSE(data.empty());
    REQUIRE(data.indices.size() % 3 == 0);
    std::filesystem::remove(path);
}

TEST_CASE("ComputeTangents produces unit tangents", "[render][meshdata]") {
    leon::MeshData data = leon::MakeCube();
    // MakeCube may already have tangents; recompute from UVs.
    for (auto& v : data.vertices) {
        v.tangent = {0.0f, 0.0f, 0.0f, 1.0f};
    }
    leon::ComputeTangents(data);
    for (const auto& v : data.vertices) {
        const float len = glm::length(glm::vec3{v.tangent});
        REQUIRE_THAT(len, WithinAbs(1.0f, 1.0e-3f));
        REQUIRE((v.tangent.w == 1.0f || v.tangent.w == -1.0f));
    }
}
