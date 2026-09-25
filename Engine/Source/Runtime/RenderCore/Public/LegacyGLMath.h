#pragma once

#include "CoreMinimal.h"

/**
 * The renderer's OpenGL conventions on Core math, until P7 switches the world to UE's axes.
 *
 * Matrices here are FMatrix values holding what glm held: column-vector transforms (M * v), stored with glm's memory
 * layout, a right-handed view and clip Z in [-1, 1]. Because the memory is the same, they are uploaded to GLSL as they
 * are (ValuePtr). In FMatrix's own row-vector product order the same composition reads backwards, so code composes
 * them with Mul(A, B), which means what glm's A * B meant. The builders reproduce glm's formulas (perspective, ortho,
 * lookAt, translate / rotate / scale), so the results match what glm produced.
 */
namespace LegacyGL
{
	/** glm's A * B (apply B, then A). */
	FORCEINLINE FMatrix Mul(const FMatrix& A, const FMatrix& B)
	{
		return B * A;
	}

	FORCEINLINE FMatrix Mul(const FMatrix& A, const FMatrix& B, const FMatrix& C)
	{
		return Mul(Mul(A, B), C);
	}

	/** glm's M * V. */
	FORCEINLINE FVector4 Transform(const FMatrix& M, const FVector4& V)
	{
		return M.TransformFVector4(V);
	}

	/** glm's vec3(M * vec4(P, 1)). */
	FORCEINLINE FVector TransformPoint(const FMatrix& M, const FVector& P)
	{
		const FVector4 R = M.TransformFVector4(FVector4(P, 1.0f));
		return FVector(R.X, R.Y, R.Z);
	}

	/** glm's vec3(M * vec4(V, 0)). */
	FORCEINLINE FVector TransformDirection(const FMatrix& M, const FVector& V)
	{
		const FVector4 R = M.TransformFVector4(FVector4(V, 0.0f));
		return FVector(R.X, R.Y, R.Z);
	}

	/** The 16 floats in upload order (glm::value_ptr). */
	FORCEINLINE const float* ValuePtr(const FMatrix& M)
	{
		return &M.M[0][0];
	}

	/** glm::radians (glm's constant, which can differ from PI / 180 in the last bit). */
	FORCEINLINE constexpr float Radians(float Degrees)
	{
		return Degrees * 0.01745329251994329576923690768489f;
	}

	/** glm::normalize: V / |V| with no tolerance. */
	FORCEINLINE FVector Normalize(const FVector& V)
	{
		return V * (1.0f / FMath::Sqrt(V | V));
	}

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
