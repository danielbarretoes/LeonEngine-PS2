#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <filesystem>
#include "Engine/Level.h"
#include "Level/LevelCatalog.h"
#include "Level/LeonLevelFormat.h"
#include <string>

using Catch::Matchers::WithinAbs;

TEST_CASE("Level stores meshes PlayerStarts and tags", "[level][container]") {
    ULevel Level;
    UStaticMeshComponent Mesh{};
    Mesh.Tag = "player";
    Mesh.Transform.Position = {1.0f, 2.0f, 3.0f};
    Level.AddStaticMesh(std::move(Mesh));

    FPlayerStart Start{};
    Start.Transform.Position = {5.0f, 0.0f, -2.0f};
    Level.AddPlayerStart(Start);

    REQUIRE(Level.GetStaticMeshes().size() == 1);
    REQUIRE(Level.FindStaticMeshIndexByTag("player") == 0);
    REQUIRE(Level.FindStaticMeshIndexByTag("missing") == ULevel::Npos);
    REQUIRE(Level.FindPlayerStart() != nullptr);
    REQUIRE_THAT(Level.FindPlayerStart()->Transform.Position.x, WithinAbs(5.0f, 1.0e-5f));

    Level.Clear();
    REQUIRE(Level.GetStaticMeshes().empty());
    REQUIRE(Level.GetPlayerStarts().empty());
    REQUIRE(Level.GetDirectionalLights().empty());
    REQUIRE(Level.FindPlayerStart() == nullptr);
}

TEST_CASE("LevelCatalog scan flat directory", "[level][catalog]") {
    const auto TempDir = std::filesystem::temp_directory_path() / "leon_level_catalog_test";
    std::filesystem::create_directories(TempDir);

    FLevelDocument Doc;
    Doc.Name = "UnitLevel";
    Doc.GameMode = "Default";
    REQUIRE(SaveLeonLevelFile((TempDir / "unit_level.llev").string(), Doc));

    FLevelCatalog Catalog;
    REQUIRE(Catalog.Scan(TempDir.string()));
    REQUIRE(Catalog.NumEntries() == 1);
    REQUIRE(Catalog.GetEntries().front().Name == "UnitLevel");
    REQUIRE(Catalog.GetEntries().front().GameMode == "Default");

    std::filesystem::remove_all(TempDir);
}
