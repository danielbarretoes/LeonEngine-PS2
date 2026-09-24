#include "Engine/Level.h"
#include "Level/BasicLight.h"
#include "Level/BasicShape.h"
#include "Level/Light.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/geometric.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("lightDirectionFromRotation round-trip", "[level][light]")
{
	const glm::vec3 Rot{45.0f, 90.0f, 0.0f};
	const glm::vec3 Dir = LightDirectionFromRotation(Rot);
	REQUIRE_THAT(glm::length(Dir), WithinAbs(1.0f, 1.0e-4f));
	const glm::vec3 Back = RotationFromLightDirection(Dir);
	REQUIRE_THAT(Back.x, WithinAbs(Rot.x, 1.0e-2f));
	REQUIRE_THAT(Back.y, WithinAbs(Rot.y, 1.0e-2f));
}

TEST_CASE("DirectionalLight GetDirection matches transform", "[level][light]")
{
	FDirectionalLight Light;
	Light.Transform.RotationDegrees = {30.0f, 0.0f, 0.0f};
	const glm::vec3 Dir = Light.GetDirection();
	REQUIRE(Dir.y < 0.0f);
}

TEST_CASE("tryParseBasicShapeName is case-insensitive", "[level][basicshape]")
{
	EBasicShape Shape{};
	REQUIRE(TryParseBasicShapeName("Cube", Shape));
	REQUIRE(Shape == EBasicShape::Cube);
	REQUIRE(TryParseBasicShapeName("sphere", Shape));
	REQUIRE(Shape == EBasicShape::Sphere);
	REQUIRE(TryParseBasicShapeName("PLANE", Shape));
	REQUIRE(Shape == EBasicShape::Plane);
	REQUIRE_FALSE(TryParseBasicShapeName("Octahedron", Shape));
}

TEST_CASE("BlockingVolume and PlayerStart name helpers", "[level][basicshape]")
{
	REQUIRE(IsBlockingVolumeName("BlockingVolume"));
	REQUIRE(IsBlockingVolumeName("blockingvolume"));
	REQUIRE(IsPlayerStartName("PlayerStart"));
	REQUIRE_FALSE(IsPlayerStartName("Cube"));
}

TEST_CASE("BasicShape factories set type and plane scale", "[level][basicshape]")
{
	const FBasicShape Cube = FBasicShape::Cube();
	REQUIRE(Cube.Type == EBasicShape::Cube);
	const FBasicShape Plane = FBasicShape::Plane(4.0f);
	REQUIRE(Plane.Type == EBasicShape::Plane);
	REQUIRE_THAT(Plane.Transform.Scale.x, WithinAbs(4.0f, 1.0e-5f));
	REQUIRE_THAT(Plane.Transform.Scale.z, WithinAbs(4.0f, 1.0e-5f));
}

TEST_CASE("BasicLight parse and addTo Level", "[level][basiclight]")
{
	EBasicLight Type{};
	REQUIRE(TryParseBasicLightName("DirectionalLight", Type));
	REQUIRE(Type == EBasicLight::Directional);
	REQUIRE(TryParseBasicLightName("PointLight", Type));
	REQUIRE(Type == EBasicLight::Point);

	ULevel Level;
	Level.ClearLights();
	REQUIRE(Level.GetDirectionalLights().empty());

	FBasicLight::Directional().AddTo(Level);
	FBasicLight::Point().AddTo(Level);
	REQUIRE(Level.GetDirectionalLights().size() == 1);
	REQUIRE(Level.GetPointLights().size() == 1);
}

TEST_CASE("Level stores meshes PlayerStarts and tags", "[level][container]")
{
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
