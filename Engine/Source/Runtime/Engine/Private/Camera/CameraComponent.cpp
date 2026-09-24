#include "Camera/CameraComponent.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace
{

	[[nodiscard]] glm::vec3 FreeLookForward(float InYawDegrees, float InPitchDegrees)
	{
		const float YawRad = glm::radians(InYawDegrees);
		const float PitchRad = glm::radians(InPitchDegrees);
		return glm::normalize(glm::vec3{
			std::cos(PitchRad) * std::cos(YawRad),
			std::sin(PitchRad),
			std::cos(PitchRad) * std::sin(YawRad),
		});
	}

	/// Stable up for lookAt when looking nearly straight up/down (ortho Top).
	[[nodiscard]] glm::vec3 FreeLookWorldUp(float InYawDegrees, float InPitchDegrees)
	{
		if (InPitchDegrees < -80.0f)
		{
			const float YawRad = glm::radians(InYawDegrees);
			return glm::normalize(glm::vec3{std::cos(YawRad), 0.0f, std::sin(YawRad)});
		}
		if (InPitchDegrees > 80.0f)
		{
			const float YawRad = glm::radians(InYawDegrees);
			return glm::normalize(glm::vec3{-std::cos(YawRad), 0.0f, -std::sin(YawRad)});
		}
		return glm::vec3{0.0f, 1.0f, 0.0f};
	}

} // namespace

void UCameraComponent::SetPerspective(float InFovDegrees, float InAspect, float InNearPlane, float InFarPlane)
{
	FovDegrees = std::clamp(InFovDegrees, 20.0f, 120.0f);
	Aspect = InAspect > 1.0e-4f ? InAspect : (16.0f / 9.0f);
	NearPlane = InNearPlane;
	FarPlane = InFarPlane;
	bOrthographic = false;
	Projection = glm::perspective(glm::radians(FovDegrees), Aspect, NearPlane, FarPlane);
}

void UCameraComponent::SetOrthographic(float Height, float InAspect, float InNearPlane, float InFarPlane)
{
	OrthoHeight = std::clamp(Height, 0.5f, 500.0f);
	Aspect = InAspect > 1.0e-4f ? InAspect : (16.0f / 9.0f);
	NearPlane = InNearPlane;
	FarPlane = InFarPlane;
	bOrthographic = true;
	const float HalfH = OrthoHeight * 0.5f;
	const float HalfW = HalfH * Aspect;
	Projection = glm::ortho(-HalfW, HalfW, -HalfH, HalfH, NearPlane, FarPlane);
}

void UCameraComponent::SetOrthoHeight(float Height)
{
	if (bOrthographic)
	{
		SetOrthographic(Height, Aspect, NearPlane, FarPlane);
	}
	else
	{
		OrthoHeight = std::clamp(Height, 0.5f, 500.0f);
	}
}

void UCameraComponent::SetFieldOfView(float InFovDegrees)
{
	SetPerspective(InFovDegrees, Aspect, NearPlane, FarPlane);
}

void UCameraComponent::SetMode(ECameraMode InMode)
{
	if (Mode == InMode)
	{
		return;
	}
	Mode = InMode;
	InvalidateCache();
}

void UCameraComponent::Orbit(float DeltaYawDegrees, float DeltaPitchDegrees)
{
	YawDegrees += DeltaYawDegrees;
	PitchDegrees = std::clamp(PitchDegrees + DeltaPitchDegrees, -89.0f, 89.0f);
	InvalidateCache();
}

void UCameraComponent::Pan(float DeltaRight, float DeltaUp)
{
	const glm::vec3 Right = RightVector();
	const glm::vec3 Up{0.0f, 1.0f, 0.0f};
	const glm::vec3 Delta = Right * DeltaRight + Up * DeltaUp;
	if (Mode == ECameraMode::FreeLook)
	{
		Eye += Delta;
	}
	else
	{
		Target += Delta;
	}
	InvalidateCache();
}

void UCameraComponent::Zoom(float DeltaDistance)
{
	if (Mode != ECameraMode::Orbit)
	{
		return;
	}
	SetDistance(Distance - DeltaDistance);
}

void UCameraComponent::SetDistance(float InDistance)
{
	Distance = std::clamp(InDistance, 0.5f, 80.0f);
	InvalidateCache();
}

void UCameraComponent::SetYawPitch(float InYawDegrees, float InPitchDegrees)
{
	YawDegrees = InYawDegrees;
	PitchDegrees = std::clamp(InPitchDegrees, -89.0f, 89.0f);
	InvalidateCache();
}

void UCameraComponent::SetTarget(const glm::vec3& InTarget)
{
	Target = InTarget;
	InvalidateCache();
}

void UCameraComponent::SetEyeLocation(const glm::vec3& InEye)
{
	Eye = InEye;
	InvalidateCache();
}

void UCameraComponent::InvalidateCache()
{
	bCacheDirty = true;
}

void UCameraComponent::UpdateCachedPosition() const
{
	if (!bCacheDirty)
	{
		return;
	}

	if (Mode == ECameraMode::FreeLook)
	{
		CachedPosition = Eye;
		bCacheDirty = false;
		return;
	}

	const float YawRad = glm::radians(YawDegrees);
	const float PitchRad = glm::radians(PitchDegrees);

	CachedPosition = Target +
		glm::vec3{
			Distance * std::cos(PitchRad) * std::cos(YawRad),
			Distance * std::sin(PitchRad),
			Distance * std::cos(PitchRad) * std::sin(YawRad),
		};
	bCacheDirty = false;
}

glm::vec3 UCameraComponent::GetCameraLocation() const
{
	UpdateCachedPosition();
	return CachedPosition;
}

glm::vec3 UCameraComponent::ForwardVector() const
{
	if (Mode == ECameraMode::FreeLook)
	{
		return FreeLookForward(YawDegrees, PitchDegrees);
	}
	UpdateCachedPosition();
	const glm::vec3 ToTarget = Target - CachedPosition;
	const float Len = glm::length(ToTarget);
	if (Len < 1.0e-5f)
	{
		return glm::vec3{0.0f, 0.0f, -1.0f};
	}
	return ToTarget / Len;
}

glm::vec3 UCameraComponent::RightVector() const
{
	const glm::vec3 Forward = ForwardVector();
	const glm::vec3 Up =
		(Mode == ECameraMode::FreeLook) ? FreeLookWorldUp(YawDegrees, PitchDegrees) : glm::vec3{0.0f, 1.0f, 0.0f};
	glm::vec3 Right = glm::cross(Forward, Up);
	const float Len = glm::length(Right);
	if (Len < 1.0e-5f)
	{
		Right = glm::cross(Forward, glm::vec3{0.0f, 0.0f, 1.0f});
		const float Len2 = glm::length(Right);
		if (Len2 < 1.0e-5f)
		{
			return glm::vec3{1.0f, 0.0f, 0.0f};
		}
		return Right / Len2;
	}
	return Right / Len;
}

glm::mat4 UCameraComponent::ViewMatrix() const
{
	UpdateCachedPosition();
	if (Mode == ECameraMode::FreeLook)
	{
		const glm::vec3 Forward = FreeLookForward(YawDegrees, PitchDegrees);
		const glm::vec3 Up = FreeLookWorldUp(YawDegrees, PitchDegrees);
		return glm::lookAt(CachedPosition, CachedPosition + Forward, Up);
	}
	return glm::lookAt(CachedPosition, Target, glm::vec3{0.0f, 1.0f, 0.0f});
}
