#include "Level/LeonLevelFormat.h"
#include "Validation/ContentValidator.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

TEST_CASE("ValidationReport counts errors and warnings", "[content][validator]")
{
	FValidationReport Report;
	REQUIRE(Report.Ok());
	REQUIRE(Report.ErrorCount() == 0);
	REQUIRE(Report.WarningCount() == 0);

	Report.Warning("a", "warn");
	REQUIRE(Report.Ok());
	REQUIRE(Report.WarningCount() == 1);

	Report.Error("b", "err");
	REQUIRE_FALSE(Report.Ok());
	REQUIRE(Report.ErrorCount() == 1);
}

namespace
{

	[[nodiscard]] FLevelActorRecord MakeCubeRecord()
	{
		FLevelActorRecord Actor;
		Actor.ActorClass = ELevelActorClass::Cube;
		return Actor;
	}

} // namespace

TEST_CASE("ValidateLevelDocument accepts minimal valid level", "[content][validator]")
{
	FLevelDocument Doc;
	Doc.Name = "Test";
	Doc.Actors.push_back(MakeCubeRecord());

	const FValidationReport Report = ValidateLevelDocument(Doc, "memory:level");
	REQUIRE(Report.Ok());
}

TEST_CASE("ValidateLevelDocument accepts a blank level", "[content][validator]")
{
	const FLevelDocument Doc;
	const FValidationReport Report = ValidateLevelDocument(Doc, "memory:blank");
	REQUIRE(Report.Ok());
}

TEST_CASE("ValidateLevelDocument checks actor mesh paths", "[content][validator]")
{
	SECTION("StaticMesh without a mesh path is an error")
	{
		FLevelDocument Doc;
		FLevelActorRecord Actor;
		Actor.ActorClass = ELevelActorClass::StaticMesh;
		Doc.Actors.push_back(Actor);

		const FValidationReport Report = ValidateLevelDocument(Doc, "memory:nomesh");
		REQUIRE_FALSE(Report.Ok());
	}
	SECTION("basic shape carrying a mesh path is an error")
	{
		FLevelDocument Doc;
		FLevelActorRecord Actor = MakeCubeRecord();
		Actor.MeshPath = "meshes/SM_Something.lmesh";
		Doc.Actors.push_back(Actor);

		const FValidationReport Report = ValidateLevelDocument(Doc, "memory:shapemesh");
		REQUIRE_FALSE(Report.Ok());
	}
	SECTION("missing mesh file is a warning, not an error")
	{
		FLevelDocument Doc;
		FLevelActorRecord Actor;
		Actor.ActorClass = ELevelActorClass::StaticMesh;
		Actor.MeshPath = "meshes/SM_DoesNotExist.lmesh";
		Doc.Actors.push_back(Actor);

		const FValidationReport Report = ValidateLevelDocument(Doc, "memory:absent");
		REQUIRE(Report.Ok());
		REQUIRE(Report.WarningCount() >= 1);
	}
}

TEST_CASE("ValidateLevelDocument rejects out-of-range values", "[content][validator]")
{
	SECTION("missing material asset")
	{
		FLevelDocument Doc;
		FLevelActorRecord Actor = MakeCubeRecord();
		Actor.MaterialPath = "Materials/M_DoesNotExist.lmat";
		Doc.Actors.push_back(Actor);

		const FValidationReport Report = ValidateLevelDocument(Doc, "memory:mat");
		REQUIRE_FALSE(Report.Ok());
	}
	SECTION("degenerate sphere tessellation")
	{
		FLevelDocument Doc;
		FLevelActorRecord Actor;
		Actor.ActorClass = ELevelActorClass::Sphere;
		Actor.SphereSegments = 1;
		Actor.SphereRings = 1;
		Doc.Actors.push_back(Actor);

		const FValidationReport Report = ValidateLevelDocument(Doc, "memory:sphere");
		REQUIRE_FALSE(Report.Ok());
	}
	SECTION("negative light intensity")
	{
		FLevelDocument Doc;
		FLevelLightRecord Light;
		Light.Intensity = -1.0f;
		Doc.Lights.push_back(Light);

		const FValidationReport Report = ValidateLevelDocument(Doc, "memory:light");
		REQUIRE_FALSE(Report.Ok());
	}
	SECTION("non-positive point light range")
	{
		FLevelDocument Doc;
		FLevelLightRecord Light;
		Light.LightClass = ELevelLightClass::PointLight;
		Light.Range = 0.0f;
		Doc.Lights.push_back(Light);

		const FValidationReport Report = ValidateLevelDocument(Doc, "memory:range");
		REQUIRE_FALSE(Report.Ok());
	}
}

TEST_CASE("Leon level bytes round-trip through the binary format", "[content][level][format]")
{
	FLevelDocument Doc;
	Doc.Name = "RoundTrip";
	Doc.GameMode = "Default";
	Doc.EnvironmentExposure = 0.75f;
	Doc.Camera.Mode = ECameraMode::FreeLook;
	Doc.Camera.Eye = {1.0f, 2.0f, 3.0f};
	Doc.Camera.Yaw = -90.0f;

	FLevelActorRecord Sphere;
	Sphere.ActorClass = ELevelActorClass::Sphere;
	Sphere.Position = {1.0f, 2.0f, 3.0f};
	Sphere.Scale = {0.5f, 0.5f, 0.5f};
	Sphere.Tag = "ball";
	Sphere.SphereSegments = 32;
	Sphere.SphereRings = 20;
	Sphere.bSimulatePhysics = true;
	Sphere.Mobility = EComponentMobility::Movable;
	Sphere.bHasBob = true;
	Sphere.BobBaseY = 2.0f;
	Doc.Actors.push_back(Sphere);

	FLevelLightRecord Point;
	Point.LightClass = ELevelLightClass::PointLight;
	Point.bHasOrbit = true;
	Point.OrbitRadius = 4.0f;
	Point.Range = 12.0f;
	Doc.Lights.push_back(Point);

	const std::vector<std::uint8_t> Bytes = SerializeLeonLevel(Doc);
	REQUIRE(Bytes.size() > 16);

	FLevelDocument Restored;
	REQUIRE(DeserializeLeonLevel(Bytes, Restored));
	REQUIRE(Restored.Name == "RoundTrip");
	REQUIRE(Restored.GameMode == "Default");
	REQUIRE(Restored.Camera.Mode == ECameraMode::FreeLook);
	REQUIRE(Restored.Actors.size() == 1);
	REQUIRE(Restored.Actors[0].ActorClass == ELevelActorClass::Sphere);
	REQUIRE(Restored.Actors[0].Tag == "ball");
	REQUIRE(Restored.Actors[0].SphereSegments == 32);
	REQUIRE(Restored.Actors[0].Mobility == EComponentMobility::Movable);
	REQUIRE(Restored.Actors[0].bHasBob);
	REQUIRE(Restored.Lights.size() == 1);
	REQUIRE(Restored.Lights[0].bHasOrbit);
}

TEST_CASE("DeserializeLeonLevel rejects bad magic and truncation", "[content][level][format]")
{
	const FLevelDocument Doc;
	std::vector<std::uint8_t> Bytes = SerializeLeonLevel(Doc);
	FLevelDocument Restored;

	SECTION("bad magic")
	{
		Bytes[0] = 'X';
		REQUIRE_FALSE(DeserializeLeonLevel(Bytes, Restored));
	}
	SECTION("truncated")
	{
		Bytes.resize(Bytes.size() / 2);
		REQUIRE_FALSE(DeserializeLeonLevel(Bytes, Restored));
	}
}

TEST_CASE("ValidateMaterialDocument checks version and types", "[content][validator]")
{
	SECTION("valid")
	{
		const nlohmann::json Doc = {
			{"version", 1},
			{"albedo", {1.0, 1.0, 1.0}},
			{"shininess", 8.0},
		};
		const FValidationReport Report = ValidateMaterialDocument(Doc, "memory:mat");
		REQUIRE(Report.Ok());
	}
	SECTION("bad albedo")
	{
		const nlohmann::json Doc = {{"version", 1}, {"albedo", "red"}};
		const FValidationReport Report = ValidateMaterialDocument(Doc, "memory:badmat");
		REQUIRE_FALSE(Report.Ok());
	}
}

TEST_CASE("ValidateMaterialFile loads M_Default.lmat", "[content][validator]")
{
#ifdef LEON_ROOT_DIR
	const std::string Path =
		(std::filesystem::path(LEON_ROOT_DIR) / "Engine/Content/Materials/M_Default.lmat").lexically_normal().string();
#else
	const std::string path = "Engine/Content/Materials/M_Default.lmat";
#endif
	const FValidationReport Report = ValidateMaterialFile(Path);
	REQUIRE(Report.Ok());
}
