#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/Axis.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Serialization/Archive.h"

/**
 * Rotation, translation and 3D scale (UE: FTransform, the scalar non-SIMD version). Applied to a point as
 * Scale, then Rotate, then Translate. A * B applies A first, then B (UE).
 */
struct CORE_API FTransform
{
	// The reflection data of the NoExport declaration (CoreUObject's NoExportTypes.h) reads the protected members'
	// offsets, as in UE.
	friend struct Z_Construct_UScriptStruct_FTransform_Statics;

protected:
	FQuat Rotation;
	FVector Translation;
	FVector Scale3D;

public:
	static const FTransform Identity;

	/** Identity (UE). */
	FORCEINLINE FTransform()
		: Rotation(0.f, 0.f, 0.f, 1.f)
		, Translation(0.f)
		, Scale3D(FVector::OneVector)
	{
	}

	FORCEINLINE explicit FTransform(const FVector& InTranslation)
		: Rotation(FQuat::Identity)
		, Translation(InTranslation)
		, Scale3D(FVector::OneVector)
	{
	}

	FORCEINLINE explicit FTransform(const FQuat& InRotation)
		: Rotation(InRotation)
		, Translation(FVector::ZeroVector)
		, Scale3D(FVector::OneVector)
	{
	}

	FORCEINLINE explicit FTransform(const FRotator& InRotation)
		: Rotation(InRotation)
		, Translation(FVector::ZeroVector)
		, Scale3D(FVector::OneVector)
	{
	}

	FORCEINLINE FTransform(
		const FQuat& InRotation, const FVector& InTranslation, const FVector& InScale3D = FVector::OneVector)
		: Rotation(InRotation)
		, Translation(InTranslation)
		, Scale3D(InScale3D)
	{
	}

	FORCEINLINE FTransform(
		const FRotator& InRotation, const FVector& InTranslation, const FVector& InScale3D = FVector::OneVector)
		: Rotation(InRotation)
		, Translation(InTranslation)
		, Scale3D(InScale3D)
	{
	}

	/** From a matrix with scale (UE). */
	explicit FORCEINLINE FTransform(const FMatrix& InMatrix)
	{
		SetFromMatrix(InMatrix);
	}

	/** From three axes and an origin (UE). */
	FTransform(const FVector& InX, const FVector& InY, const FVector& InZ, const FVector& InTranslation);

	FString ToHumanReadableString() const;
	FString ToString() const;
	bool InitFromString(const FString& InSourceString);

	/** Matrix with the scale applied (UE: ToMatrixWithScale). */
	FMatrix ToMatrixWithScale() const;

	/** Inverse of ToMatrixWithScale (UE: ToInverseMatrixWithScale). */
	FMatrix ToInverseMatrixWithScale() const;

	/** Matrix of the rotation and translation only (UE: ToMatrixNoScale). */
	FMatrix ToMatrixNoScale() const;

	/** The inverse transform; exact without non-uniform scale on a rotated transform (UE: Inverse). */
	FTransform Inverse() const;

	/** Linear blend of two transforms, slerp-free (UE: Blend). */
	void Blend(const FTransform& Atom1, const FTransform& Atom2, float Alpha);

	/** Sets this to the matrix decomposition (UE: SetFromMatrix). */
	void SetFromMatrix(const FMatrix& InMatrix);

	/** This, then Other (UE: operator*). */
	FTransform operator*(const FTransform& Other) const;
	void operator*=(const FTransform& Other);

	/** Rotation applied on top: this * FTransform(Other) (UE: operator*(FQuat)). */
	FTransform operator*(const FQuat& Other) const;
	void operator*=(const FQuat& Other);

	/** Out = A then B (UE: Multiply). */
	static void Multiply(FTransform* OutTransform, const FTransform* A, const FTransform* B);

	/** The transform of this relative to Other: this = Result * Other (UE: GetRelativeTransform). */
	FTransform GetRelativeTransform(const FTransform& Other) const;

	/** Other relative to this: Other * this^-1 (UE: GetRelativeTransformReverse). */
	FTransform GetRelativeTransformReverse(const FTransform& Other) const;

	/** Applies the transform to a homogeneous vector (UE: TransformFVector4). */
	FVector4 TransformFVector4(const FVector4& V) const;
	FVector4 TransformFVector4NoScale(const FVector4& V) const;

	/** Scale, rotate and translate a point (UE: TransformPosition). */
	FORCEINLINE FVector TransformPosition(const FVector& V) const
	{
		return Rotation.RotateVector(Scale3D * V) + Translation;
	}
	FORCEINLINE FVector TransformPositionNoScale(const FVector& V) const
	{
		return Rotation.RotateVector(V) + Translation;
	}

	/** Scale and rotate a direction (UE: TransformVector). */
	FORCEINLINE FVector TransformVector(const FVector& V) const
	{
		return Rotation.RotateVector(Scale3D * V);
	}
	FORCEINLINE FVector TransformVectorNoScale(const FVector& V) const
	{
		return Rotation.RotateVector(V);
	}

	FVector InverseTransformPosition(const FVector& V) const;
	FVector InverseTransformPositionNoScale(const FVector& V) const;
	FVector InverseTransformVector(const FVector& V) const;
	FVector InverseTransformVectorNoScale(const FVector& V) const;

	FORCEINLINE FQuat TransformRotation(const FQuat& Q) const
	{
		return GetRotation() * Q;
	}
	FORCEINLINE FQuat InverseTransformRotation(const FQuat& Q) const
	{
		return GetRotation().Inverse() * Q;
	}

	/** Scale multiplied by a uniform factor (UE: GetScaled). */
	FTransform GetScaled(float Scale) const;
	FTransform GetScaled(FVector Scale) const;

	/** Axis with the scale (UE: GetScaledAxis). */
	FVector GetScaledAxis(EAxis::Type InAxis) const;
	FVector GetUnitAxis(EAxis::Type InAxis) const;

	/** Mirrors across an axis (UE: Mirror). */
	void Mirror(EAxis::Type MirrorAxis, EAxis::Type FlipAxis);

	/** 1 / scale, with SMALL_NUMBER components becoming 0 (UE: GetSafeScaleReciprocal). */
	static FVector GetSafeScaleReciprocal(const FVector& InScale, float Tolerance = SMALL_NUMBER);

	FORCEINLINE FVector GetLocation() const
	{
		return GetTranslation();
	}

	FORCEINLINE FRotator Rotator() const
	{
		return Rotation.Rotator();
	}

	/** Determinant of the scale (UE: GetDeterminant). */
	FORCEINLINE float GetDeterminant() const
	{
		return Scale3D.X * Scale3D.Y * Scale3D.Z;
	}

	FORCEINLINE void SetLocation(const FVector& Origin)
	{
		Translation = Origin;
	}

	bool ContainsNaN() const
	{
		return Translation.ContainsNaN() || Rotation.ContainsNaN() || Scale3D.ContainsNaN();
	}

	FORCEINLINE bool IsValid() const
	{
		if (ContainsNaN())
		{
			return false;
		}
		if (!Rotation.IsNormalized())
		{
			return false;
		}
		return true;
	}

	/** Rotations equal within Tolerance (UE: AreRotationsEqual and siblings). */
	FORCEINLINE static bool AreRotationsEqual(
		const FTransform& A, const FTransform& B, float Tolerance = KINDA_SMALL_NUMBER)
	{
		return A.Rotation.Equals(B.Rotation, Tolerance);
	}
	FORCEINLINE static bool AreTranslationsEqual(
		const FTransform& A, const FTransform& B, float Tolerance = KINDA_SMALL_NUMBER)
	{
		return A.Translation.Equals(B.Translation, Tolerance);
	}
	FORCEINLINE static bool AreScale3DsEqual(
		const FTransform& A, const FTransform& B, float Tolerance = KINDA_SMALL_NUMBER)
	{
		return A.Scale3D.Equals(B.Scale3D, Tolerance);
	}

	FORCEINLINE bool RotationEquals(const FTransform& Other, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return AreRotationsEqual(*this, Other, Tolerance);
	}
	FORCEINLINE bool TranslationEquals(const FTransform& Other, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return AreTranslationsEqual(*this, Other, Tolerance);
	}
	FORCEINLINE bool Scale3DEquals(const FTransform& Other, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return AreScale3DsEqual(*this, Other, Tolerance);
	}

	FORCEINLINE bool Equals(const FTransform& Other, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return RotationEquals(Other, Tolerance) && TranslationEquals(Other, Tolerance) &&
			Scale3DEquals(Other, Tolerance);
	}

	FORCEINLINE bool EqualsNoScale(const FTransform& Other, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return RotationEquals(Other, Tolerance) && TranslationEquals(Other, Tolerance);
	}

	FORCEINLINE void SetComponents(const FQuat& InRotation, const FVector& InTranslation, const FVector& InScale3D)
	{
		Rotation = InRotation;
		Translation = InTranslation;
		Scale3D = InScale3D;
	}

	FORCEINLINE void SetIdentity()
	{
		Rotation = FQuat::Identity;
		Translation = FVector::ZeroVector;
		Scale3D = FVector(1, 1, 1);
	}

	FORCEINLINE void MultiplyScale3D(const FVector& Scale3DMultiplier)
	{
		Scale3D *= Scale3DMultiplier;
	}

	FORCEINLINE void SetTranslation(const FVector& NewTranslation)
	{
		Translation = NewTranslation;
	}

	FORCEINLINE void AddToTranslation(const FVector& DeltaTranslation)
	{
		Translation += DeltaTranslation;
	}

	FORCEINLINE void ConcatenateRotation(const FQuat& DeltaRotation)
	{
		Rotation = Rotation * DeltaRotation;
	}

	FORCEINLINE void SetRotation(const FQuat& NewRotation)
	{
		Rotation = NewRotation;
	}

	FORCEINLINE void SetScale3D(const FVector& NewScale3D)
	{
		Scale3D = NewScale3D;
	}

	FORCEINLINE void NormalizeRotation()
	{
		Rotation.Normalize();
	}

	FORCEINLINE bool IsRotationNormalized() const
	{
		return Rotation.IsNormalized();
	}

	FORCEINLINE FQuat GetRotation() const
	{
		return Rotation;
	}

	FORCEINLINE FVector GetTranslation() const
	{
		return Translation;
	}

	FORCEINLINE FVector GetScale3D() const
	{
		return Scale3D;
	}

	FORCEINLINE void CopyRotationPart(const FTransform& SrcBA)
	{
		Rotation = SrcBA.Rotation;
		Scale3D = SrcBA.Scale3D;
	}

	FORCEINLINE void CopyTranslationAndScale3D(const FTransform& SrcBA)
	{
		Translation = SrcBA.Translation;
		Scale3D = SrcBA.Scale3D;
	}

	FORCEINLINE float GetMaximumAxisScale() const
	{
		return Scale3D.GetAbsMax();
	}

	FORCEINLINE float GetMinimumAxisScale() const
	{
		return Scale3D.GetAbsMin();
	}

	/** Rotation, translation, scale (UE). */
	friend FArchive& operator<<(FArchive& Ar, FTransform& M)
	{
		return Ar << M.Rotation << M.Translation << M.Scale3D;
	}

private:
	static FORCEINLINE bool AnyHasNegativeScale(const FVector& InScale3D, const FVector& InOtherScale3D)
	{
		return InScale3D.X < 0.f || InScale3D.Y < 0.f || InScale3D.Z < 0.f || InOtherScale3D.X < 0.f ||
			InOtherScale3D.Y < 0.f || InOtherScale3D.Z < 0.f;
	}

	static void MultiplyUsingMatrixWithScale(FTransform* OutTransform, const FTransform* A, const FTransform* B);

	/** Multiply path for a negative scale: goes through matrices (UE: ConstructTransformFromMatrixWithDesiredScale). */
	static void ConstructTransformFromMatrixWithDesiredScale(
		const FMatrix& AMatrix, const FMatrix& BMatrix, const FVector& DesiredScale, FTransform& OutTransform);

	static void GetRelativeTransformUsingMatrixWithScale(
		FTransform* OutTransform, const FTransform* Base, const FTransform* Relative);
};

FORCEINLINE uint32 GetTypeHash(const FTransform& Transform)
{
	return HashCombine(HashCombine(GetTypeHash(Transform.GetRotation()), GetTypeHash(Transform.GetTranslation())),
		GetTypeHash(Transform.GetScale3D()));
}
