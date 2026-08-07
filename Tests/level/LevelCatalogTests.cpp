#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <filesystem>
#include <leon/level/Level.h>
#include <leon/level/LevelCatalog.h>
#include <leon/level/LeonLevelFormat.h>
#include <string>

using Catch::Matchers::WithinAbs;

TEST_CASE("Level stores meshes PlayerStarts and tags", "[level][container]") {
    leon::Level level;
    leon::StaticMeshComponent mesh{};
    mesh.tag = "player";
    mesh.transform.position = {1.0f, 2.0f, 3.0f};
    level.AddStaticMesh(std::move(mesh));

    leon::PlayerStart start{};
    start.transform.position = {5.0f, 0.0f, -2.0f};
    level.AddPlayerStart(start);

    REQUIRE(level.StaticMeshes().size() == 1);
    REQUIRE(level.FindStaticMeshIndexByTag("player") == 0);
    REQUIRE(level.FindStaticMeshIndexByTag("missing") == leon::Level::npos);
    REQUIRE(level.FindPlayerStart() != nullptr);
    REQUIRE_THAT(level.FindPlayerStart()->transform.position.x, WithinAbs(5.0f, 1.0e-5f));

    level.Clear();
    REQUIRE(level.StaticMeshes().empty());
    REQUIRE(level.PlayerStarts().empty());
    REQUIRE(level.DirectionalLights().empty());
    REQUIRE(level.FindPlayerStart() == nullptr);
}

TEST_CASE("LevelCatalog ScanProjectPacks finds Smoke levels", "[level][catalog]") {
#ifdef LEON_SOURCE_DIR
    const std::string gamesRoot =
        (std::filesystem::path(LEON_SOURCE_DIR) / "Projects").lexically_normal().string();
#else
    const std::string gamesRoot = "Projects";
#endif
    leon::LevelCatalog catalog;
    REQUIRE(catalog.ScanProjectPacks(gamesRoot));
    REQUIRE_FALSE(catalog.IsEmpty());

    bool foundSmoke = false;
    for (const leon::LevelEntry& entry : catalog.Entries()) {
        if (entry.pack == "Smoke") {
            foundSmoke = true;
            REQUIRE_FALSE(entry.path.empty());
            break;
        }
    }
    REQUIRE(foundSmoke);
}

TEST_CASE("LevelCatalog scan flat directory", "[level][catalog]") {
    const auto tempDir = std::filesystem::temp_directory_path() / "leon_level_catalog_test";
    std::filesystem::create_directories(tempDir);

    leon::LevelDocument doc;
    doc.name = "UnitLevel";
    doc.gameMode = "Default";
    REQUIRE(leon::SaveLeonLevelFile((tempDir / "unit_level.llev").string(), doc));

    leon::LevelCatalog catalog;
    REQUIRE(catalog.Scan(tempDir.string()));
    REQUIRE(catalog.NumEntries() == 1);
    REQUIRE(catalog.Entries().front().name == "UnitLevel");
    REQUIRE(catalog.Entries().front().gameMode == "Default");

    std::filesystem::remove_all(tempDir);
}
