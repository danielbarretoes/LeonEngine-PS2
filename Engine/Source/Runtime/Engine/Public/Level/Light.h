#pragma once

#include "Math/Transform.h"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

constexpr int MaxDirectionalLights = 2;
constexpr int MaxPointLights = 4;

/// Unreal FDirectionalLight Source Angle default (~sun disc), in degrees.
constexpr float DefaultLightSourceAngleDegrees = 0.5357f;

/// Light travel direction from Unreal-like pitch (X) / yaw (Y) degrees. Roll ignored.
[[nodiscard]] inline glm::vec3 LightDirectionFromRotation(const glm::vec3& RotationDegrees)
{
	constexpr float DegToRad = 0.017453292519943295769f;
	const float Pitch = RotationDegrees.x * DegToRad;
	const float Yaw = RotationDegrees.y * DegToRad;
	const float Cp = std::cos(Pitch);
	const glm::vec3 Dir{std::sin(Yaw) * Cp, -std::sin(Pitch), std::cos(Yaw) * Cp};
	const float Len = glm::length(Dir);
	return Len > 1.0e-8f ? (Dir / Len) : glm::vec3{0.0f, -1.0f, 0.0f};
}

/// Inverse of `lightDirectionFromRotation` (roll = 0).
[[nodiscard]] inline glm::vec3 RotationFromLightDirection(const glm::vec3& Direction)
{
	constexpr float RadToDeg = 57.295779513082320877f;
	const float Len = glm::length(Direction);
	const glm::vec3 D = Len > 1.0e-8f ? (Direction / Len) : glm::vec3{0.0f, -1.0f, 0.0f};
	const float Pitch = std::asin(std::clamp(-D.y, -1.0f, 1.0f));
	const float Yaw = std::atan2(D.x, D.z);
	return {Pitch * RadToDeg, Yaw * RadToDeg, 0.0f};
}

/// Unreal-like FDirectionalLight: transform drives aim; no raw direction field.
struct ENGINE_API FDirectionalLight
{
	FTransform Transform{{0.0f, 0.0f, 0.0f}, {60.3f, 142.1f, 0.0f}, {1.0f, 1.0f, 1.0f}};
	glm::vec3 LightColor{1.0f, 1.0f, 1.0f};
	float Intensity = 1.0f;
	bool bCastShadows = true;
	float SourceAngle = DefaultLightSourceAngleDegrees;

	[[nodiscard]] glm::vec3 GetDirection() const
	{
		return LightDirectionFromRotation(Transform.RotationDegrees);
	}
};

/// Unreal-like FPointLight: location from transform; attenuation `range`.
struct ENGINE_API FPointLight
{
	FTransform Transform{{0.0f, 2.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
	glm::vec3 LightColor{1.0f, 1.0f, 1.0f};
	float Intensity = 1.0f;
	float Range = 8.0f;
	bool bCastShadows = false;

	/// Optional orbit animation (Level JSON `orbit`); preserved for save round-trip.
	bool bHasOrbit = false;
	float OrbitRadius = 1.0f;
	float OrbitHeight = 1.0f;
	float OrbitHeightAmp = 0.0f;
	float OrbitSpeed = 1.0f;
};
