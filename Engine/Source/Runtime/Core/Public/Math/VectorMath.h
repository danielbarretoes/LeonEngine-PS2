#pragma once

#include "CoreTypes.h"
#include "Math/MathFwd.h"

/**
 * Planes packed four to a quadword for the vector units (Leon; UE keeps its frustum's planes the same way in
 * FConvexVolume::PermutedPlanes): the X, Y, Z and W of up to MaxPlanes planes, each a row of MaxPlanes floats, plus
 * the absolute value of X, Y and Z (a box's reach along the plane's normal). A plane is inside when
 * X * P.X + Y * P.Y + Z * P.Z + W >= 0; the unused slots are always inside.
 */
struct alignas(16) CORE_API FVectorPlaneSet
{
	/** Two quadwords a component: 8 planes (a frustum's 6, or 4 of a portal's narrowing and the near and far). */
	static constexpr int32 MaxPlanes = 8;

	float X[MaxPlanes];
	float Y[MaxPlanes];
	float Z[MaxPlanes];
	float W[MaxPlanes];
	float AbsX[MaxPlanes];
	float AbsY[MaxPlanes];
	float AbsZ[MaxPlanes];
	int32 NumPlanes = 0;

	FVectorPlaneSet();

	/** Adds a plane (A, B, C, D: inside where A x + B y + C z + D >= 0); at most MaxPlanes. */
	void AddPlane(float A, float B, float C, float D);
};

/**
 * The scalar reference of the vector math (Leon; UE: Math/UnrealMathFPU.h): what FVectorMath does on every platform
 * without a vector unit, and what the PS2's VU0 is compared against (TestPAL, System.Core.Math.VectorMathVU0).
 */
struct CORE_API FVectorMathFPU
{
	/** Result = A * B with UE's row vectors: Result[R][C] = sum over K of A[R][K] * B[K][C], K from 0 to 3 in order. */
	static void MatrixMultiply(FMatrix& Result, const FMatrix& A, const FMatrix& B);
	/** P * M: the row vector P times the matrix (UE: FMatrix::TransformFVector4). */
	static void TransformVector4(FVector4& Result, const FMatrix& M, const FVector4& P);
	/**
	 * Whether the box of Center and Extent is wholly outside one of the planes: the plane's distance to the centre plus
	 * the box's reach along its normal is below 0.
	 */
	[[nodiscard]] static bool IsBoxOutside(const FVectorPlaneSet& Planes, const FVector& Center, const FVector& Extent);
	/** Whether the sphere is wholly outside one of the planes (their normals must be unit: distances in its units). */
	[[nodiscard]] static bool IsSphereOutside(const FVectorPlaneSet& Planes, const FVector& Center, float Radius);
};

/**
 * The vector math of the platform (Leon, Docs/PLANS/ps2-shipping.md N15; UE: VectorMatrixMultiply,
 * VectorTransformVector, FConvexVolume's permuted planes): on the PS2 VU0 in macro mode (COP2: LQC2, VMULA, VMADDA,
 * SQC2 from the EE), elsewhere FVectorMathFPU. FMatrix's product and transform, the renderer's culling and the skeletal
 * pose's matrices go through it.
 *
 * VU0 is not IEEE: it truncates where the EE's FPU also truncates, keeps no denormals and clamps instead of making
 * infinities, and it accumulates in ACC. TestPAL compares it with the reference within two units in the last place of
 * the sum of the magnitudes of each sum's products (System.Core.Math.VectorMathVU0).
 */
struct CORE_API FVectorMath
{
	static void MatrixMultiply(FMatrix& Result, const FMatrix& A, const FMatrix& B);
	static void TransformVector4(FVector4& Result, const FMatrix& M, const FVector4& P);
	[[nodiscard]] static bool IsBoxOutside(const FVectorPlaneSet& Planes, const FVector& Center, const FVector& Extent);
	[[nodiscard]] static bool IsSphereOutside(const FVectorPlaneSet& Planes, const FVector& Center, float Radius);
	/** True where FVectorMath runs on a vector unit (the PS2's VU0), false where it is FVectorMathFPU. */
	[[nodiscard]] static bool IsVectorUnit();
};
