#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include "Validation/ContentValidator.h"
#include "Level/LeonLevelFormat.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

TEST_CASE("ValidationReport counts errors and warnings", "[content][validator]") {
    FValidationReport report;
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

[[nodiscard]] FLevelActorRecord MakeCubeRecord() {
    FLevelActorRecord actor;
    actor.actorClass = ELevelActorClass::Cube;
    return actor;
}

} // namespace

TEST_CASE("ValidateLevelDocument accepts minimal valid level", "[content][validator]") {
    FLevelDocument doc;
    doc.name = "Test";
    doc.actors.push_back(MakeCubeRecord());

    const FValidationReport report = ValidateLevelDocument(doc, "memory:level");
    REQUIRE(report.ok());
}

TEST_CASE("ValidateLevelDocument accepts a blank level", "[content][validator]") {
    const FLevelDocument doc;
    const FValidationReport report = ValidateLevelDocument(doc, "memory:blank");
    REQUIRE(report.ok());
}

TEST_CASE("ValidateLevelDocument checks actor mesh paths", "[content][validator]") {
    SECTION("StaticMesh without a mesh path is an error") {
        FLevelDocument doc;
        FLevelActorRecord actor;
        actor.actorClass = ELevelActorClass::StaticMesh;
        doc.actors.push_back(actor);

        const FValidationReport report = ValidateLevelDocument(doc, "memory:nomesh");
        REQUIRE_FALSE(report.ok());
    }
    SECTION("basic shape carrying a mesh path is an error") {
        FLevelDocument doc;
        FLevelActorRecord actor = MakeCubeRecord();
        actor.meshPath = "meshes/SM_Something.lmesh";
        doc.actors.push_back(actor);

        const FValidationReport report = ValidateLevelDocument(doc, "memory:shapemesh");
        REQUIRE_FALSE(report.ok());
    }
    SECTION("missing mesh file is a warning, not an error") {
        FLevelDocument doc;
        FLevelActorRecord actor;
        actor.actorClass = ELevelActorClass::StaticMesh;
        actor.meshPath = "meshes/SM_DoesNotExist.lmesh";
        doc.actors.push_back(actor);

        const FValidationReport report = ValidateLevelDocument(doc, "memory:absent");
        REQUIRE(report.ok());
        REQUIRE(report.warningCount() >= 1);
    }
}

TEST_CASE("ValidateLevelDocument rejects out-of-range values", "[content][validator]") {
    SECTION("missing material asset") {
        FLevelDocument doc;
        FLevelActorRecord actor = MakeCubeRecord();
        actor.materialPath = "Materials/M_DoesNotExist.lmat";
        doc.actors.push_back(actor);

        const FValidationReport report = ValidateLevelDocument(doc, "memory:mat");
        REQUIRE_FALSE(report.ok());
    }
    SECTION("degenerate sphere tessellation") {
        FLevelDocument doc;
        FLevelActorRecord actor;
        actor.actorClass = ELevelActorClass::Sphere;
        actor.sphereSegments = 1;
        actor.sphereRings = 1;
        doc.actors.push_back(actor);

        const FValidationReport report = ValidateLevelDocument(doc, "memory:sphere");
        REQUIRE_FALSE(report.ok());
    }
    SECTION("negative light intensity") {
        FLevelDocument doc;
        FLevelLightRecord light;
        light.intensity = -1.0f;
        doc.lights.push_back(light);

        const FValidationReport report = ValidateLevelDocument(doc, "memory:light");
        REQUIRE_FALSE(report.ok());
    }
    SECTION("non-positive point light range") {
        FLevelDocument doc;
        FLevelLightRecord light;
        light.lightClass = ELevelLightClass::PointLight;
        light.range = 0.0f;
        doc.lights.push_back(light);

        const FValidationReport report = ValidateLevelDocument(doc, "memory:range");
        REQUIRE_FALSE(report.ok());
    }
}

TEST_CASE("Leon level bytes round-trip through the binary format", "[content][level][format]") {
    FLevelDocument doc;
    doc.name = "RoundTrip";
    doc.gameMode = "Default";
    doc.environmentExposure = 0.75f;
    doc.camera.mode = ECameraMode::FreeLook;
    doc.camera.eye = {1.0f, 2.0f, 3.0f};
    doc.camera.yaw = -90.0f;

    FLevelActorRecord sphere;
    sphere.actorClass = ELevelActorClass::Sphere;
    sphere.position = {1.0f, 2.0f, 3.0f};
    sphere.scale = {0.5f, 0.5f, 0.5f};
    sphere.tag = "ball";
    sphere.sphereSegments = 32;
    sphere.sphereRings = 20;
    sphere.simulatePhysics = true;
    sphere.mobility = EComponentMobility::Movable;
    sphere.hasBob = true;
    sphere.bobBaseY = 2.0f;
    doc.actors.push_back(sphere);

    FLevelLightRecord point;
    point.lightClass = ELevelLightClass::PointLight;
    point.hasOrbit = true;
    point.orbitRadius = 4.0f;
    point.range = 12.0f;
    doc.lights.push_back(point);

    const std::vector<std::uint8_t> bytes = SerializeLeonLevel(doc);
    REQUIRE(bytes.size() > 16);

    FLevelDocument restored;
    REQUIRE(DeserializeLeonLevel(bytes, restored));
    REQUIRE(restored.name == "RoundTrip");
    REQUIRE(restored.gameMode == "Default");
    REQUIRE(restored.camera.mode == ECameraMode::FreeLook);
    REQUIRE(restored.actors.size() == 1);
    REQUIRE(restored.actors[0].actorClass == ELevelActorClass::Sphere);
    REQUIRE(restored.actors[0].tag == "ball");
    REQUIRE(restored.actors[0].sphereSegments == 32);
    REQUIRE(restored.actors[0].mobility == EComponentMobility::Movable);
    REQUIRE(restored.actors[0].hasBob);
    REQUIRE(restored.lights.size() == 1);
    REQUIRE(restored.lights[0].hasOrbit);
}

TEST_CASE("DeserializeLeonLevel rejects bad magic and truncation", "[content][level][format]") {
    const FLevelDocument doc;
    std::vector<std::uint8_t> bytes = SerializeLeonLevel(doc);
    FLevelDocument restored;

    SECTION("bad magic") {
        bytes[0] = 'X';
        REQUIRE_FALSE(DeserializeLeonLevel(bytes, restored));
    }
    SECTION("truncated") {
        bytes.resize(bytes.size() / 2);
        REQUIRE_FALSE(DeserializeLeonLevel(bytes, restored));
    }
}

TEST_CASE("ValidateMaterialDocument checks version and types", "[content][validator]") {
    SECTION("valid") {
        const nlohmann::json doc = {
            {"version", 1},
            {"albedo", {1.0, 1.0, 1.0}},
            {"shininess", 8.0},
        };
        const FValidationReport report = ValidateMaterialDocument(doc, "memory:mat");
        REQUIRE(report.ok());
    }
    SECTION("bad albedo") {
        const nlohmann::json doc = {{"version", 1}, {"albedo", "red"}};
        const FValidationReport report = ValidateMaterialDocument(doc, "memory:badmat");
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
    const FValidationReport report = ValidateMaterialFile(path);
    REQUIRE(report.ok());
}
