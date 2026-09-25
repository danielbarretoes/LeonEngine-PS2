#pragma once

// Explicit conversions between Core math and glm for the modules that still use glm (P3 to P6). Desktop only; the
// header and glm go away in P6. The values are copied as they are: no axis or unit conversion happens here.

#include "CoreTypes.h"
#include "Math/Color.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstring>

// FMatrix is row-major for row vectors (V * M); glm::mat4 is column-major for column vectors (M * v). The same
// transform therefore has the same 16 floats in the same order, and the conversion is a copy.
static_assert(sizeof(FMatrix) == sizeof(glm::mat4), "FMatrix and glm::mat4 must have the same size");

FORCEINLINE glm::vec2 ToGlm(const FVector2D& V)
{
	return glm::vec2(V.X, V.Y);
}

FORCEINLINE glm::vec3 ToGlm(const FVector& V)
{
	return glm::vec3(V.X, V.Y, V.Z);
}

FORCEINLINE glm::vec4 ToGlm(const FVector4& V)
{
	return glm::vec4(V.X, V.Y, V.Z, V.W);
}

FORCEINLINE glm::vec4 ToGlm(const FLinearColor& C)
{
	return glm::vec4(C.R, C.G, C.B, C.A);
}

FORCEINLINE glm::quat ToGlm(const FQuat& Q)
{
	glm::quat Result;
	Result.x = Q.X;
	Result.y = Q.Y;
	Result.z = Q.Z;
	Result.w = Q.W;
	return Result;
}

FORCEINLINE glm::mat4 ToGlm(const FMatrix& M)
{
	glm::mat4 Result;
	std::memcpy(&Result, &M.M[0][0], sizeof(Result));
	return Result;
}

FORCEINLINE FVector2D FromGlm(const glm::vec2& V)
{
	return FVector2D(V.x, V.y);
}

FORCEINLINE FVector FromGlm(const glm::vec3& V)
{
	return FVector(V.x, V.y, V.z);
}

FORCEINLINE FVector4 FromGlm(const glm::vec4& V)
{
	return FVector4(V.x, V.y, V.z, V.w);
}

FORCEINLINE FQuat FromGlm(const glm::quat& Q)
{
	return FQuat(Q.x, Q.y, Q.z, Q.w);
}

FORCEINLINE FMatrix FromGlm(const glm::mat4& M)
{
	FMatrix Result;
	std::memcpy(&Result.M[0][0], &M, sizeof(Result.M));
	return Result;
}
