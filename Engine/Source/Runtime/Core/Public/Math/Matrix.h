#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/Axis.h"
#include "Math/MathFwd.h"
#include "Math/Plane.h"
#include "Math/Rotator.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Serialization/Archive.h"

/**
 * 4x4 matrix, row-major, for row vectors: V' = V * M, translation in M[3][0..2], axes in rows 0..2 (UE: FMatrix).
 * A * B applies A first, then B. The memory layout matches GLSL's column-major mat4 of the same transform, so the
 * renderer uploads it as is.
 */
struct alignas(16) CORE_API FMatrix
{
	float M[4][4];

	static const FMatrix Identity;

	/** Uninitialised (UE). */
	FMatrix() = default;

	/** Zero matrix (UE: FMatrix(EForceInit)). */
	explicit FORCEINLINE FMatrix(EForceInit)
		: M{}
	{
	}

	/** Rows. */
	FMatrix(const FPlane& InX, const FPlane& InY, const FPlane& InZ, const FPlane& InW);

	/** Rows with the W column (0, 0, 0, 1). */
	FMatrix(const FVector& InX, const FVector& InY, const FVector& InZ, const FVector& InW);

	void SetIdentity();

	/** This, then Other. */
	FMatrix operator*(const FMatrix& Other) const;
	void operator*=(const FMatrix& Other);

	FMatrix operator+(const FMatrix& Other) const;
	void operator+=(const FMatrix& Other);

	FMatrix operator*(float Other) const;
	void operator*=(float Other);

	bool operator==(const FMatrix& Other) const;
	FORCEINLINE bool operator!=(const FMatrix& Other) const
	{
		return !(*this == Other);
	}

	bool Equals(const FMatrix& Other, float Tolerance = KINDA_SMALL_NUMBER) const;

	FVector4 TransformFVector4(const FVector4& V) const;

	/** (V, 1) * M (UE: TransformPosition). */
	FVector4 TransformPosition(const FVector& V) const;

	/** Inverse(M) applied to a position (UE: InverseTransformPosition). */
	FVector InverseTransformPosition(const FVector& V) const;

	/** (V, 0) * M: no translation (UE: TransformVector). */
	FVector4 TransformVector(const FVector& V) const;

	FVector InverseTransformVector(const FVector& V) const;

	FMatrix GetTransposed() const;

	float Determinant() const;

	/** Determinant of the 3x3 rotation part (UE: RotDeterminant). */
	float RotDeterminant() const;

	/** Inverse without checks; a singular matrix gives infinities (UE: InverseFast). */
	FMatrix InverseFast() const;

	/** Inverse; a singular or zero-scale matrix gives Identity (UE: Inverse). */
	FMatrix Inverse() const;

	/** Transpose of the 3x3 adjoint, for transforming normals (UE: TransposeAdjoint). */
	FMatrix TransposeAdjoint() const;

	/** Makes the axis rows unit length (UE: RemoveScaling). */
	void RemoveScaling(float Tolerance = SMALL_NUMBER);
	FMatrix GetMatrixWithoutScale(float Tolerance = SMALL_NUMBER) const;

	/** Removes the scale and returns it (UE: ExtractScaling). */
	FVector ExtractScaling(float Tolerance = SMALL_NUMBER);

	/** Lengths of the axis rows (UE: GetScaleVector). */
	FVector GetScaleVector(float Tolerance = SMALL_NUMBER) const;

	/** Without the translation row (UE: RemoveTranslation). */
	FMatrix RemoveTranslation() const;

	/** Adds a translation (UE: ConcatTranslation). */
	FMatrix ConcatTranslation(const FVector& Translation) const;

	bool ContainsNaN() const;

	void ScaleTranslation(const FVector& Scale3D);

	float GetMinimumAxisScale() const;
	float GetMaximumAxisScale() const;

	/** Scaled copy of the 3x3 part (translation too, like UE: ApplyScale). */
	FMatrix ApplyScale(float Scale) const;

	FORCEINLINE FVector GetOrigin() const
	{
		return FVector(M[3][0], M[3][1], M[3][2]);
	}

	FVector GetScaledAxis(EAxis::Type Axis) const;
	void GetScaledAxes(FVector& X, FVector& Y, FVector& Z) const;
	FVector GetUnitAxis(EAxis::Type Axis) const;
	void GetUnitAxes(FVector& X, FVector& Y, FVector& Z) const;

	/** Sets axis row i (0..2). */
	void SetAxis(int32 i, const FVector& Axis);
	void SetOrigin(const FVector& NewOrigin);
	void SetAxes(
		FVector* Axis0 = nullptr, FVector* Axis1 = nullptr, FVector* Axis2 = nullptr, FVector* Origin = nullptr);

	FVector GetColumn(int32 i) const;
	void SetColumn(int32 i, FVector Value);

	/** The rotation part as a rotator (UE: Rotator). */
	FRotator Rotator() const;

	/** The rotation part as a quaternion; the matrix must not be scaled (UE: ToQuat). */
	FQuat ToQuat() const;

	/** Frustum planes of a view-projection matrix, pointing outward (UE: GetFrustum*Plane). */
	bool GetFrustumNearPlane(FPlane& OutPlane) const;
	bool GetFrustumFarPlane(FPlane& OutPlane) const;
	bool GetFrustumLeftPlane(FPlane& OutPlane) const;
	bool GetFrustumRightPlane(FPlane& OutPlane) const;
	bool GetFrustumTopPlane(FPlane& OutPlane) const;
	bool GetFrustumBottomPlane(FPlane& OutPlane) const;

	/** Mirrors across an axis and flips another to keep the handedness (UE: Mirror). */
	void Mirror(EAxis::Type MirrorAxis, EAxis::Type FlipAxis);

	FString ToString() const;

	/** Hash of the 16 floats (UE: ComputeHash). */
	uint32 ComputeHash() const;
};

FORCEINLINE uint32 GetTypeHash(const FMatrix& Matrix)
{
	return Matrix.ComputeHash();
}

/** View matrix from an eye position and a look direction (UE: FLookFromMatrix). */
struct CORE_API FLookFromMatrix : public FMatrix
{
	FLookFromMatrix(const FVector& EyePosition, const FVector& LookDirection, const FVector& UpVector);
};

/** View matrix looking from EyePosition at LookAtPosition (UE: FLookAtMatrix). */
struct CORE_API FLookAtMatrix : public FLookFromMatrix
{
	FLookAtMatrix(const FVector& EyePosition, const FVector& LookAtPosition, const FVector& UpVector)
		: FLookFromMatrix(EyePosition, LookAtPosition - EyePosition, UpVector)
	{
	}
};

inline FArchive& operator<<(FArchive& Ar, FMatrix& M)
{
	for (int32 Row = 0; Row < 4; ++Row)
	{
		Ar << M.M[Row][0] << M.M[Row][1] << M.M[Row][2] << M.M[Row][3];
	}
	return Ar;
}
