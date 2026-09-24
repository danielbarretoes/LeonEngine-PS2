#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Material.h"
#include "MaterialAsset.h"
#include <nlohmann/json.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("roughnessFromShininess decreases with shininess", "[render][material]") {
    const float roughSoft = roughnessFromShininess(8.0f);
    const float roughHard = roughnessFromShininess(256.0f);
    REQUIRE(roughSoft > roughHard);
    REQUIRE(roughHard >= 0.04f);
    REQUIRE(roughSoft <= 1.0f);
}

TEST_CASE("Material isTransparent uses alpha threshold", "[render][material]") {
    Material mat;
    mat.alpha = 1.0f;
    REQUIRE_FALSE(mat.isTransparent());
    mat.alpha = 0.5f;
    REQUIRE(mat.isTransparent());
}

TEST_CASE("Material syncRoughnessFromShininess", "[render][material]") {
    Material mat;
    mat.shininess = 128.0f;
    mat.syncRoughnessFromShininess();
    REQUIRE_THAT(mat.roughness, WithinAbs(roughnessFromShininess(128.0f), 1.0e-6f));
}

TEST_CASE("HasMaterialSurfaceFields detects surface keys", "[render][material]") {
    REQUIRE(HasMaterialSurfaceFields({{"albedo", {1, 1, 1}}}));
    REQUIRE(HasMaterialSurfaceFields({{"albedoMap", "checker"}}));
    REQUIRE_FALSE(HasMaterialSurfaceFields({{"tag", "player"}}));
    REQUIRE_FALSE(HasMaterialSurfaceFields(nlohmann::json::object()));
}
