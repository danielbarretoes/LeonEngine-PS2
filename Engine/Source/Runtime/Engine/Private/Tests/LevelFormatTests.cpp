#include "Level/LeonLevelFormat.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

TEST_CASE("Leon level bytes round-trip through the binary format", "[content][level][format]")
{
	FLevelDocument Doc;
	Doc.Name = "RoundTrip";
	Doc.GameMode = "Default";
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
