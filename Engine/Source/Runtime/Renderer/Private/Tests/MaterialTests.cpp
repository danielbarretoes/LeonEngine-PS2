#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Material.h"
#include "MaterialAsset.h"
#include <nlohmann/json.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("roughnessFromShininess decreases with shininess", "[render][material]") {
    const float RoughSoft = RoughnessFromShininess(8.0f);
    const float RoughHard = RoughnessFromShininess(256.0f);
    REQUIRE(RoughSoft > RoughHard);
    REQUIRE(RoughHard >= 0.04f);
    REQUIRE(RoughSoft <= 1.0f);
}

TEST_CASE("Material isTransparent uses alpha threshold", "[render][material]") {
    FMaterial Mat;
    Mat.Alpha = 1.0f;
    REQUIRE_FALSE(Mat.IsTransparent());
    Mat.Alpha = 0.5f;
    REQUIRE(Mat.IsTransparent());
}

TEST_CASE("Material syncRoughnessFromShininess", "[render][material]") {
    FMaterial Mat;
    Mat.Shininess = 128.0f;
    Mat.SyncRoughnessFromShininess();
    REQUIRE_THAT(Mat.Roughness, WithinAbs(RoughnessFromShininess(128.0f), 1.0e-6f));
}

TEST_CASE("HasMaterialSurfaceFields detects surface keys", "[render][material]") {
    REQUIRE(HasMaterialSurfaceFields({{"albedo", {1, 1, 1}}}));
    REQUIRE(HasMaterialSurfaceFields({{"albedoMap", "checker"}}));
    REQUIRE_FALSE(HasMaterialSurfaceFields({{"tag", "player"}}));
    REQUIRE_FALSE(HasMaterialSurfaceFields(nlohmann::json::object()));
}
