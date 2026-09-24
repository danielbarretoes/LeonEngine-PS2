#include "GameFramework/Input.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>

namespace
{

	constexpr float DegToRad = glm::pi<float>() / 180.0f;

} // namespace

glm::vec3 YawRelativeMoveXz(float YawDegrees, const FMoveAxes2D& Axes)
{
	if (!Axes.Any())
	{
		return glm::vec3{0.0f};
	}

	const float YawRad = YawDegrees * DegToRad;
	// Match Camera orbit: world offset ≈ (cos(p)*cos(y), …, cos(p)*sin(y)); look ≈ -offset.xz
	const glm::vec3 Forward{-std::cos(YawRad), 0.0f, -std::sin(YawRad)};
	const glm::vec3 Right{std::sin(YawRad), 0.0f, -std::cos(YawRad)};

	glm::vec3 Move = (Forward * Axes.Z) + (Right * Axes.X);
	const float Len = glm::length(Move);
	if (Len > 1.0e-4f)
	{
		Move /= Len;
	}
	return Move;
}

glm::vec3 CameraRelativeMoveXz(const UCameraComponent& Camera, const FMoveAxes2D& Axes)
{
	return YawRelativeMoveXz(Camera.GetYawDegrees(), Axes);
}
