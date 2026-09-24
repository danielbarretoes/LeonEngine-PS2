#include "Camera/ArenaCamera.h"

#include "Camera/CameraComponent.h"

#include <glm/common.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>

namespace
{

	[[nodiscard]] float DistXZ(const glm::vec3& A, const glm::vec3& B)
	{
		const float Dx = A.x - B.x;
		const float Dz = A.z - B.z;
		return std::sqrt(Dx * Dx + Dz * Dz);
	}

	[[nodiscard]] float ExpAlpha(float Speed, float Dt)
	{
		return (Speed <= 0.0f || Dt <= 0.0f) ? 1.0f : (1.0f - std::exp(-Speed * Dt));
	}

} // namespace

void UpdateArenaCamera(UCameraComponent& Camera, FArenaCameraState& State, const FArenaCameraParams& Params,
	const std::vector<glm::vec3>& LivingFeet, float DeltaTime, float FloorYFallback)
{
	glm::vec3 DesiredTarget = State.Target;
	float DesiredDistance = State.Distance;

	if (!LivingFeet.empty())
	{
		glm::vec3 Sum{0.0f};
		for (const glm::vec3& Feet : LivingFeet)
		{
			Sum += Feet + glm::vec3{0.0f, Params.TargetHeightOffset, 0.0f};
		}
		DesiredTarget = Sum / static_cast<float>(LivingFeet.size());

		float MaxSep = 0.0f;
		for (const glm::vec3& Feet : LivingFeet)
		{
			const glm::vec3 Focus = Feet + glm::vec3{0.0f, Params.TargetHeightOffset, 0.0f};
			MaxSep = (std::max)(MaxSep, DistXZ(Focus, DesiredTarget));
		}
		DesiredDistance = std::clamp(
			Params.DistanceBase + MaxSep * Params.DistancePerSeparation, Params.MinDistance, Params.MaxDistance);
	}
	else
	{
		// No living pawns: frame arena center at standing height; distance ≈ prior Furytoon empty
		// framing (distanceBase * 2 → 18 with defaults).
		DesiredTarget = {0.0f, FloorYFallback + Params.TargetHeightOffset, 0.0f};
		DesiredDistance = std::clamp(Params.DistanceBase * 2.0f, Params.MinDistance, Params.MaxDistance);
	}

	const float A = ExpAlpha(Params.LagSpeed, DeltaTime);
	State.Target = glm::mix(State.Target, DesiredTarget, A);
	State.Distance = glm::mix(State.Distance, DesiredDistance, A);

	Camera.SetMode(ECameraMode::Orbit);
	Camera.SetTarget(State.Target);
	Camera.SetDistance(State.Distance);
	Camera.SetYawPitch(Params.FixedYawDegrees, Params.FixedPitchDegrees);
}
