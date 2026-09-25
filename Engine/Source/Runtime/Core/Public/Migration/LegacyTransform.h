#pragma once

#include "CoreTypes.h"

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

/// TRS transform used to build model / normal matrices (the pre-UE glm transform; replaced by the native FTransform in
/// P6).
struct CORE_API FLegacyTransform
{
	glm::vec3 Position{0.0f};
	glm::vec3 RotationDegrees{0.0f}; // XYZ Euler, degrees
	glm::vec3 Scale{1.0f};

	[[nodiscard]] glm::mat4 ModelMatrix() const;
	[[nodiscard]] glm::mat3 NormalMatrix() const;
};
