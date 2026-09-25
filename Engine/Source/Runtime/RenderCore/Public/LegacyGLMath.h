#pragma once

#include "CoreMinimal.h"

/**
 * The renderer's OpenGL matrix builders, until P7 switches the world to UE's axes.
 *
 * They reproduce glm's formulas (perspective, ortho, lookAt, translate / rotate / scale, mat4_cast) term by term, so
 * the results match what glm produced: a right-handed view and clip Z in [-1, 1]. glm stored column-vector matrices;
 * the same 16 floats read as an FMatrix are the row-vector matrix of the same transform, so the results compose with
 * FMatrix's own operators (A * B applies A first) and upload to GLSL as they are.
 */
namespace LegacyGL
{
	/** glm::perspective (right-handed, clip Z in [-1, 1]). */
	RENDERCORE_API FMatrix Perspective(float FovYRadians, float Aspect, float ZNear, float ZFar);

	/** glm::ortho (right-handed, clip Z in [-1, 1]). */
	RENDERCORE_API FMatrix Ortho(float Left, float Right, float Bottom, float Top, float ZNear, float ZFar);

	/** glm::lookAt (right-handed). */
	RENDERCORE_API FMatrix LookAt(const FVector& Eye, const FVector& Center, const FVector& Up);

	/** glm::translate(M, V). */
	RENDERCORE_API FMatrix Translate(const FMatrix& M, const FVector& V);

	/** glm::rotate(M, Radians, Axis). */
	RENDERCORE_API FMatrix Rotate(const FMatrix& M, float Radians, const FVector& Axis);

	/** glm::scale(M, V). */
	RENDERCORE_API FMatrix Scale(const FMatrix& M, const FVector& V);

	/** glm::mat4_cast: the rotation of the unit quaternion (W, X, Y, Z), term by term like glm. */
	RENDERCORE_API FMatrix QuatToMatrix(float W, float X, float Y, float Z);

	/**
	 * The normal matrix transpose(inverse(mat3(Model))) in glm's mat3 memory order (9 floats, uploaded with
	 * glUniformMatrix3fv); the identity when mat3(Model) is singular.
	 */
	RENDERCORE_API void NormalMatrix3x3(const FMatrix& Model, float Out[9]);
} // namespace LegacyGL
