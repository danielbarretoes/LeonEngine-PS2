#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include "Validation/ContentValidator.h"
#include "Level/LeonLevelFormat.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

TEST_CASE("ValidationReport counts errors and warnings", "[content][validator]") {
    leon::ValidationReport report;
    REQUIRE(report.ok());
    REQUIRE(report.errorCount() == 0);
    REQUIRE(report.warningCount() == 0);

    report.warning("a", "warn");
    REQUIRE(report.ok());
    REQUIRE(report.warningCount() == 1);

    report.error("b", "err");
    REQUIRE_FALSE(report.ok());
    REQUIRE(report.errorCount() == 1);
}

namespace {

[[nodiscard]] leon::LevelActorRecord MakeCubeRecord() {
    leon::LevelActorRecord actor;
    actor.actorClass = leon::ELevelActorClass::Cube;
    return actor;
}

} // namespace

TEST_CASE("ValidateLevelDocument accepts minimal valid level", "[content][validator]") {
    leon::LevelDocument doc;
    doc.name = "Test";
    doc.actors.push_back(MakeCubeRecord());

    const leon::ValidationReport report = leon::ValidateLevelDocument(doc, "memory:level");
    REQUIRE(report.ok());
}

TEST_CASE("ValidateLevelDocument accepts a blank level", "[content][validator]") {
    const leon::LevelDocument doc;
    const leon::ValidationReport report = leon::ValidateLevelDocument(doc, "memory:blank");
    REQUIRE(report.ok());
}

TEST_CASE("ValidateLevelDocument checks actor mesh paths", "[content][validator]") {
    SECTION("StaticMesh without a mesh path is an error") {
        leon::LevelDocument doc;
        leon::LevelActorRecord actor;
        actor.actorClass = leon::ELevelActorClass::StaticMesh;
        doc.actors.push_back(actor);

        const leon::ValidationReport report = leon::ValidateLevelDocument(doc, "memory:nomesh");
        REQUIRE_FALSE(report.ok());
    }
    SECTION("basic shape carrying a mesh path is an error") {
        leon::LevelDocument doc;
        leon::LevelActorRecord actor = MakeCubeRecord();
        actor.meshPath = "meshes/SM_Something.lmesh";
        doc.actors.push_back(actor);

        const leon::ValidationReport report = leon::ValidateLevelDocument(doc, "memory:shapemesh");
        REQUIRE_FALSE(report.ok());
    }
    SECTION("missing mesh file is a warning, not an error") {
        leon::LevelDocument doc;
        leon::LevelActorRecord actor;
        actor.actorClass = leon::ELevelActorClass::StaticMesh;
        actor.meshPath = "meshes/SM_DoesNotExist.lmesh";
        doc.actors.push_back(actor);

        const leon::ValidationReport report = leon::ValidateLevelDocument(doc, "memory:absent");
        REQUIRE(report.ok());
        REQUIRE(report.warningCount() >= 1);
    }
}

TEST_CASE("ValidateLevelDocument rejects out-of-range values", "[content][validator]") {
    SECTION("missing material asset") {
        leon::LevelDocument doc;
        leon::LevelActorRecord actor = MakeCubeRecord();
        actor.materialPath = "Materials/M_DoesNotExist.lmat";
        doc.actors.push_back(actor);

        const leon::ValidationReport report = leon::ValidateLevelDocument(doc, "memory:mat");
        REQUIRE_FALSE(report.ok());
    }
    SECTION("degenerate sphere tessellation") {
        leon::LevelDocument doc;
        leon::LevelActorRecord actor;
        actor.actorClass = leon::ELevelActorClass::Sphere;
        actor.sphereSegments = 1;
        actor.sphereRings = 1;
        doc.actors.push_back(actor);

        const leon::ValidationReport report = leon::ValidateLevelDocument(doc, "memory:sphere");
        REQUIRE_FALSE(report.ok());
    }
    SECTION("negative light intensity") {
        leon::LevelDocument doc;
        leon::LevelLightRecord light;
        light.intensity = -1.0f;
        doc.lights.push_back(light);

        const leon::ValidationReport report = leon::ValidateLevelDocument(doc, "memory:light");
        REQUIRE_FALSE(report.ok());
    }
    SECTION("non-positive point light range") {
        leon::LevelDocument doc;
        leon::LevelLightRecord light;
        light.lightClass = leon::ELevelLightClass::PointLight;
        light.range = 0.0f;
        doc.lights.push_back(light);

        const leon::ValidationReport report = leon::ValidateLevelDocument(doc, "memory:range");
        REQUIRE_FALSE(report.ok());
    }
}

TEST_CASE("Leon level bytes round-trip through the binary format", "[content][level][format]") {
    leon::LevelDocument doc;
    doc.name = "RoundTrip";
    doc.gameMode = "Default";
    doc.environmentExposure = 0.75f;
    doc.camera.mode = leon::ECameraMode::FreeLook;
    doc.camera.eye = {1.0f, 2.0f, 3.0f};
    doc.camera.yaw = -90.0f;

    leon::LevelActorRecord sphere;
    sphere.actorClass = leon::ELevelActorClass::Sphere;
    sphere.position = {1.0f, 2.0f, 3.0f};
    sphere.scale = {0.5f, 0.5f, 0.5f};
    sphere.tag = "ball";
    sphere.sphereSegments = 32;
    sphere.sphereRings = 20;
    sphere.simulatePhysics = true;
    sphere.mobility = leon::EComponentMobility::Movable;
    sphere.hasBob = true;
    sphere.bobBaseY = 2.0f;
    doc.actors.push_back(sphere);

    leon::LevelLightRecord point;
    point.lightClass = leon::ELevelLightClass::PointLight;
    point.hasOrbit = true;
    point.orbitRadius = 4.0f;
    point.range = 12.0f;
    doc.lights.push_back(point);

    const std::vector<std::uint8_t> bytes = leon::SerializeLeonLevel(doc);
    REQUIRE(bytes.size() > 16);

    leon::LevelDocument restored;
    REQUIRE(leon::DeserializeLeonLevel(bytes, restored));
    REQUIRE(restored.name == "RoundTrip");
    REQUIRE(restored.gameMode == "Default");
    REQUIRE(restored.camera.mode == leon::ECameraMode::FreeLook);
    REQUIRE(restored.actors.size() == 1);
    REQUIRE(restored.actors[0].actorClass == leon::ELevelActorClass::Sphere);
    REQUIRE(restored.actors[0].tag == "ball");
    REQUIRE(restored.actors[0].sphereSegments == 32);
    REQUIRE(restored.actors[0].mobility == leon::EComponentMobility::Movable);
    REQUIRE(restored.actors[0].hasBob);
    REQUIRE(restored.lights.size() == 1);
    REQUIRE(restored.lights[0].hasOrbit);
}

TEST_CASE("DeserializeLeonLevel rejects bad magic and truncation", "[content][level][format]") {
    const leon::LevelDocument doc;
    std::vector<std::uint8_t> bytes = leon::SerializeLeonLevel(doc);
    leon::LevelDocument restored;

    SECTION("bad magic") {
        bytes[0] = 'X';
        REQUIRE_FALSE(leon::DeserializeLeonLevel(bytes, restored));
    }
    SECTION("truncated") {
        bytes.resize(bytes.size() / 2);
        REQUIRE_FALSE(leon::DeserializeLeonLevel(bytes, restored));
    }
}

TEST_CASE("ValidateMaterialDocument checks version and types", "[content][validator]") {
    SECTION("valid") {
        const nlohmann::json doc = {
            {"version", 1},
            {"albedo", {1.0, 1.0, 1.0}},
            {"shininess", 8.0},
        };
        const leon::ValidationReport report = leon::ValidateMaterialDocument(doc, "memory:mat");
        REQUIRE(report.ok());
    }
    SECTION("bad albedo") {
        const nlohmann::json doc = {{"version", 1}, {"albedo", "red"}};
        const leon::ValidationReport report = leon::ValidateMaterialDocument(doc, "memory:badmat");
        REQUIRE_FALSE(report.ok());
    }
}

TEST_CASE("ValidateMaterialFile loads M_Default.lmat", "[content][validator]") {
#ifdef LEON_ROOT_DIR
    const std::string path =
        (std::filesystem::path(LEON_ROOT_DIR) / "Engine/Content/Materials/M_Default.lmat")
            .lexically_normal()
            .string();
#else
    const std::string path = "Engine/Content/Materials/M_Default.lmat";
#endif
    const leon::ValidationReport report = leon::ValidateMaterialFile(path);
    REQUIRE(report.ok());
}
