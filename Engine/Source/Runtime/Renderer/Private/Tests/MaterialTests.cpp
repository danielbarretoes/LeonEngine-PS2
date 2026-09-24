#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Material.h"
#include "MaterialAsset.h"
#include <nlohmann/json.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("roughnessFromShininess decreases with shininess", "[render][material]") {
    const float roughSoft = leon::roughnessFromShininess(8.0f);
    const float roughHard = leon::roughnessFromShininess(256.0f);
    REQUIRE(roughSoft > roughHard);
    REQUIRE(roughHard >= 0.04f);
    REQUIRE(roughSoft <= 1.0f);
}

TEST_CASE("Material isTransparent uses alpha threshold", "[render][material]") {
    leon::Material mat;
    mat.alpha = 1.0f;
    REQUIRE_FALSE(mat.isTransparent());
    mat.alpha = 0.5f;
    REQUIRE(mat.isTransparent());
}

TEST_CASE("Material syncRoughnessFromShininess", "[render][material]") {
    leon::Material mat;
    mat.shininess = 128.0f;
    mat.syncRoughnessFromShininess();
    REQUIRE_THAT(mat.roughness, WithinAbs(leon::roughnessFromShininess(128.0f), 1.0e-6f));
}

TEST_CASE("HasMaterialSurfaceFields detects surface keys", "[render][material]") {
    REQUIRE(leon::HasMaterialSurfaceFields({{"albedo", {1, 1, 1}}}));
    REQUIRE(leon::HasMaterialSurfaceFields({{"albedoMap", "checker"}}));
    REQUIRE_FALSE(leon::HasMaterialSurfaceFields({{"tag", "player"}}));
    REQUIRE_FALSE(leon::HasMaterialSurfaceFields(nlohmann::json::object()));
}
