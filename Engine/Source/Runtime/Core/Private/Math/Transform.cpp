#include "Math/Transform.h"

#include "Math/Vector4.h"
#include "MathStringParsing.h"

// FTransform, scalar version (UE: Math/TransformNonVectorized.h and Math/Transform.cpp).

namespace
{
	/** Blend weights below this pick one of the two transforms outright (UE: ZERO_ANIMWEIGHT_THRESH). */
	constexpr float ZeroAnimWeightThresh = 0.00001f;
} // namespace

const FTransform FTransform::Identity(FQuat(0.f, 0.f, 0.f, 1.f), FVector(0.f), FVector(1.f));

FTransform::FTransform(const FVector& InX, const FVector& InY, const FVector& InZ, const FVector& InTranslation)
{
	SetFromMatrix(FMatrix(InX, InY, InZ, InTranslation));
}

FString FTransform::ToHumanReadableString() const
{
	const FRotator R(GetRotation());
	const FVector T(GetTranslation());
	const FVector S(GetScale3D());

	FString Output =
		FString::Printf("Rotation: Pitch %f Yaw %f Roll %f\r\n", double(R.Pitch), double(R.Yaw), double(R.Roll));
	Output += FString::Printf("Translation: %f %f %f\r\n", double(T.X), double(T.Y), double(T.Z));
	Output += FString::Printf("Scale3D: %f %f %f\r\n", double(S.X), double(S.Y), double(S.Z));

	return Output;
}

FString FTransform::ToString() const
{
	const FRotator R(Rotator());
	const FVector T(GetTranslation());
	const FVector S(GetScale3D());

	return FString::Printf("%f,%f,%f|%f,%f,%f|%f,%f,%f", double(T.X), double(T.Y), double(T.Z), double(R.Pitch),
		double(R.Yaw), double(R.Roll), double(S.X), double(S.Y), double(S.Z));
}

bool FTransform::InitFromString(const FString& InSourceString)
{
	// "Tx,Ty,Tz|Pitch,Yaw,Roll|Sx,Sy,Sz", the ToString format.
	const TCHAR* Stream = *InSourceString;
	FVector ParsedTranslation;
	FRotator ParsedRotation;
	FVector ParsedScale;

	if (!MathStringParsing::ParseFloatTriple(Stream, ParsedTranslation.X, ParsedTranslation.Y, ParsedTranslation.Z) ||
		*Stream++ != '|')
	{
		return false;
	}
	if (!MathStringParsing::ParseFloatTriple(Stream, ParsedRotation.Pitch, ParsedRotation.Yaw, ParsedRotation.Roll) ||
		*Stream++ != '|')
	{
		return false;
	}
	if (!MathStringParsing::ParseFloatTriple(Stream, ParsedScale.X, ParsedScale.Y, ParsedScale.Z) || *Stream != '\0')
	{
		return false;
	}

	SetComponents(ParsedRotation.Quaternion(), ParsedTranslation, ParsedScale);
	return true;
}

FMatrix FTransform::ToMatrixWithScale() const
{
	FMatrix OutMatrix;

	OutMatrix.M[3][0] = Translation.X;
	OutMatrix.M[3][1] = Translation.Y;
	OutMatrix.M[3][2] = Translation.Z;

	const float X2 = Rotation.X + Rotation.X;
	const float Y2 = Rotation.Y + Rotation.Y;
	const float Z2 = Rotation.Z + Rotation.Z;
	{
		const float XX2 = Rotation.X * X2;
		const float YY2 = Rotation.Y * Y2;
		const float ZZ2 = Rotation.Z * Z2;

		OutMatrix.M[0][0] = (1.0f - (YY2 + ZZ2)) * Scale3D.X;
		OutMatrix.M[1][1] = (1.0f - (XX2 + ZZ2)) * Scale3D.Y;
		OutMatrix.M[2][2] = (1.0f - (XX2 + YY2)) * Scale3D.Z;
	}
	{
		const float YZ2 = Rotation.Y * Z2;
		const float WX2 = Rotation.W * X2;

		OutMatrix.M[2][1] = (YZ2 - WX2) * Scale3D.Z;
		OutMatrix.M[1][2] = (YZ2 + WX2) * Scale3D.Y;
	}
	{
		const float XY2 = Rotation.X * Y2;
		const float WZ2 = Rotation.W * Z2;

		OutMatrix.M[1][0] = (XY2 - WZ2) * Scale3D.Y;
		OutMatrix.M[0][1] = (XY2 + WZ2) * Scale3D.X;
	}
	{
		const float XZ2 = Rotation.X * Z2;
		const float WY2 = Rotation.W * Y2;

		OutMatrix.M[2][0] = (XZ2 + WY2) * Scale3D.Z;
		OutMatrix.M[0][2] = (XZ2 - WY2) * Scale3D.X;
	}

	OutMatrix.M[0][3] = 0.0f;
	OutMatrix.M[1][3] = 0.0f;
	OutMatrix.M[2][3] = 0.0f;
	OutMatrix.M[3][3] = 1.0f;

	return OutMatrix;
}

FMatrix FTransform::ToInverseMatrixWithScale() const
{
	// Invert the scale / rotation / translation matrix to be safe with non-uniform scale.
	return ToMatrixWithScale().Inverse();
}

FMatrix FTransform::ToMatrixNoScale() const
{
	FTransform NoScale = *this;
	NoScale.Scale3D = FVector(1.0f);
	return NoScale.ToMatrixWithScale();
}

FTransform FTransform::Inverse() const
{
	const FQuat InvRotation = Rotation.Inverse();
	// This used to cause NaN if Scale contained 0.
	const FVector InvScale3D = GetSafeScaleReciprocal(Scale3D);
	const FVector InvTranslation = InvRotation * (InvScale3D * -Translation);

	return FTransform(InvRotation, InvTranslation, InvScale3D);
}

void FTransform::Blend(const FTransform& Atom1, const FTransform& Atom2, float Alpha)
{
	if (Alpha <= ZeroAnimWeightThresh)
	{
		// If blend is all the way for child1, then just copy its bone atoms.
		*this = Atom1;
	}
	else if (Alpha >= 1.f - ZeroAnimWeightThresh)
	{
		// If blend is all the way for child2, then just copy its bone atoms.
		*this = Atom2;
	}
	else
	{
		// Simple linear interpolation for translation and scale.
		Translation = FMath::Lerp(Atom1.Translation, Atom2.Translation, Alpha);
		Scale3D = FMath::Lerp(Atom1.Scale3D, Atom2.Scale3D, Alpha);
		Rotation = FQuat::FastLerp(Atom1.Rotation, Atom2.Rotation, Alpha);

		// ...and renormalize.
		Rotation.Normalize();
	}
}

void FTransform::SetFromMatrix(const FMatrix& InMatrix)
{
	FMatrix M = InMatrix;

	// Get the 3D scale from the matrix.
	Scale3D = M.ExtractScaling();

	// If there is negative scaling going on, we handle that here.
	if (InMatrix.Determinant() < 0.f)
	{
		// Assume it is along X and modify transform accordingly. It doesn't actually matter which axis we choose,
		// the 'appearance' will be the same.
		Scale3D.X *= -1.f;
		M.SetAxis(0, -M.GetScaledAxis(EAxis::X));
	}

	Rotation = FQuat(M);
	Translation = InMatrix.GetOrigin();

	// Normalize rotation.
	Rotation.Normalize();
}

FTransform FTransform::operator*(const FTransform& Other) const
{
	FTransform Output;
	Multiply(&Output, this, &Other);
	return Output;
}

void FTransform::operator*=(const FTransform& Other)
{
	Multiply(this, this, &Other);
}

FTransform FTransform::operator*(const FQuat& Other) const
{
	FTransform Output;
	const FTransform OtherTransform(Other, FVector::ZeroVector, FVector::OneVector);
	Multiply(&Output, this, &OtherTransform);
	return Output;
}

void FTransform::operator*=(const FQuat& Other)
{
	const FTransform OtherTransform(Other, FVector::ZeroVector, FVector::OneVector);
	Multiply(this, this, &OtherTransform);
}

void FTransform::Multiply(FTransform* OutTransform, const FTransform* A, const FTransform* B)
{
	checkSlow(A->IsRotationNormalized());
	checkSlow(B->IsRotationNormalized());

	//	When Q = quaternion, S = single scalar scale, and T = translation
	//	QST(A) = Q(A), S(A), T(A), and QST(B) = Q(B), S(B), T(B)

	//	QST (AxB)

	// QST(A) = Q(A)*S(A)*P*-Q(A) + T(A)
	// QST(AxB) = Q(B)*S(B)*QST(A)*-Q(B) + T(B)
	// QST(AxB) = Q(B)*S(B)*[Q(A)*S(A)*P*-Q(A) + T(A)]*-Q(B) + T(B)
	// QST(AxB) = Q(B)*S(B)*Q(A)*S(A)*P*-Q(A)*-Q(B) + Q(B)*S(B)*T(A)*-Q(B) + T(B)
	// QST(AxB) = [Q(B)*Q(A)]*[S(B)*S(A)]*P*-[Q(B)*Q(A)] + Q(B)*S(B)*T(A)*-Q(B) + T(B)

	//	Q(AxB) = Q(B)*Q(A)
	//	S(AxB) = S(A)*S(B)
	//	T(AxB) = Q(B)*S(B)*T(A)*-Q(B) + T(B)

	if (AnyHasNegativeScale(A->Scale3D, B->Scale3D))
	{
		// @note, if you have 0 scale with negative, you're going to lose rotation as it can't convert back to quat.
		MultiplyUsingMatrixWithScale(OutTransform, A, B);
	}
	else
	{
		// Computed into locals first: OutTransform may alias A or B.
		const FQuat NewRotation = B->Rotation * A->Rotation;
		const FVector NewScale3D = A->Scale3D * B->Scale3D;
		const FVector NewTranslation = B->Rotation * (B->Scale3D * A->Translation) + B->Translation;

		OutTransform->Rotation = NewRotation;
		OutTransform->Scale3D = NewScale3D;
		OutTransform->Translation = NewTranslation;
	}
}

void FTransform::MultiplyUsingMatrixWithScale(FTransform* OutTransform, const FTransform* A, const FTransform* B)
{
	// The goal of using M is to get the correct orientation, but for translation, we still need scale.
	ConstructTransformFromMatrixWithDesiredScale(
		A->ToMatrixWithScale(), B->ToMatrixWithScale(), A->Scale3D * B->Scale3D, *OutTransform);
}

void FTransform::ConstructTransformFromMatrixWithDesiredScale(
	const FMatrix& AMatrix, const FMatrix& BMatrix, const FVector& DesiredScale, FTransform& OutTransform)
{
	// The goal of using M is to get the correct orientation, but for translation, we still need scale.
	FMatrix M = AMatrix * BMatrix;
	M.RemoveScaling();

	// Apply negative scale back to axes.
	const FVector SignedScale = DesiredScale.GetSignVector();

	M.SetAxis(0, SignedScale.X * M.GetScaledAxis(EAxis::X));
	M.SetAxis(1, SignedScale.Y * M.GetScaledAxis(EAxis::Y));
	M.SetAxis(2, SignedScale.Z * M.GetScaledAxis(EAxis::Z));

	// @note: if you have negative with 0 scale, this will return rotation that is identity since matrix loses that
	// axis.
	FQuat NewRotation = FQuat(M);
	NewRotation.Normalize();

	// Set values back to output.
	OutTransform.Scale3D = DesiredScale;
	OutTransform.Rotation = NewRotation;

	// Technically I could calculate this using FTransform but then it does more quat multiplication instead of using
	// Scale in matrix multiplication; it's a question of between RemoveScaling vs using FTransform to move translation.
	OutTransform.Translation = M.GetOrigin();
}

FTransform FTransform::GetRelativeTransform(const FTransform& Other) const
{
	// A * B(-1) = VQS(B)(-1) (VQS (A))
	//
	// Scale = S(A)/S(B)
	// Rotation = Q(B)(-1) * Q(A)
	// Translation = 1/S(B) *[Q(B)(-1)*(T(A)-T(B))*Q(B)]
	// where A = this, B = Other
	FTransform Result;

	if (AnyHasNegativeScale(Scale3D, Other.GetScale3D()))
	{
		// @note, if you have 0 scale with negative, you're going to lose rotation as it can't convert back to quat.
		GetRelativeTransformUsingMatrixWithScale(&Result, this, &Other);
	}
	else
	{
		const FVector SafeRecipScale3D = GetSafeScaleReciprocal(Other.Scale3D, SMALL_NUMBER);
		Result.Scale3D = Scale3D * SafeRecipScale3D;

		if (!Other.Rotation.IsNormalized())
		{
			return FTransform::Identity;
		}

		const FQuat InverseRot = Other.Rotation.Inverse();
		Result.Rotation = InverseRot * Rotation;

		Result.Translation = (InverseRot * (Translation - Other.Translation)) * (SafeRecipScale3D);
	}

	return Result;
}

void FTransform::GetRelativeTransformUsingMatrixWithScale(
	FTransform* OutTransform, const FTransform* Base, const FTransform* Relative)
{
	// The goal of using M is to get the correct orientation, but for translation, we still need scale.
	const FMatrix AM = Base->ToMatrixWithScale();
	const FMatrix BM = Relative->ToMatrixWithScale();
	// Get combined scale.
	const FVector SafeRecipScale3D = GetSafeScaleReciprocal(Relative->Scale3D, SMALL_NUMBER);
	const FVector DesiredScale3D = Base->Scale3D * SafeRecipScale3D;
	ConstructTransformFromMatrixWithDesiredScale(AM, BM.Inverse(), DesiredScale3D, *OutTransform);
}

FTransform FTransform::GetRelativeTransformReverse(const FTransform& Other) const
{
	// A (-1) * B = VQS(B)(VQS (A)(-1))
	//
	// Scale = S(B)/S(A)
	// Rotation = Q(B) * Q(A)(-1)
	// Translation = T(B)-S(B)/S(A) *[Q(B)*Q(A)(-1)*T(A)*Q(A)*Q(B)(-1)]
	// where A = this, and B = Other
	FTransform Result;

	const FQuat RotationInverse = Rotation.Inverse();
	const FVector SafeRecipScale3D = GetSafeScaleReciprocal(Scale3D);
	Result.Scale3D = Other.Scale3D * SafeRecipScale3D;
	Result.Rotation = Other.Rotation * RotationInverse;
	Result.Translation = Other.Translation - Result.Scale3D * (Result.Rotation * Translation);

	return Result;
}

FVector4 FTransform::TransformFVector4(const FVector4& V) const
{
	// If not, this won't work.
	checkSlow(V.W == 0.f || V.W == 1.f);

	// Transform using QST is following: QST(P) = Q*S*P*-Q + T where Q = quaternion, S = scale, T = translation.
	FVector4 Result = FVector4(Rotation.RotateVector(Scale3D * FVector(V)), 0.f);
	if (V.W == 1.f)
	{
		Result += FVector4(Translation, 1.f);
	}

	return Result;
}

FVector4 FTransform::TransformFVector4NoScale(const FVector4& V) const
{
	// If not, this won't work.
	checkSlow(V.W == 0.f || V.W == 1.f);

	// Transform using QST is following: QST(P) = Q*S*P*-Q + T where Q = quaternion, S = 1.0f, T = translation.
	FVector4 Result = FVector4(Rotation.RotateVector(FVector(V)), 0.f);
	if (V.W == 1.f)
	{
		Result += FVector4(Translation, 1.f);
	}

	return Result;
}

FVector FTransform::InverseTransformPosition(const FVector& V) const
{
	return (Rotation.UnrotateVector(V - Translation)) * GetSafeScaleReciprocal(Scale3D);
}

FVector FTransform::InverseTransformPositionNoScale(const FVector& V) const
{
	return (Rotation.UnrotateVector(V - Translation));
}

FVector FTransform::InverseTransformVector(const FVector& V) const
{
	return (Rotation.UnrotateVector(V)) * GetSafeScaleReciprocal(Scale3D);
}

FVector FTransform::InverseTransformVectorNoScale(const FVector& V) const
{
	return (Rotation.UnrotateVector(V));
}

FTransform FTransform::GetScaled(float InScale) const
{
	FTransform A(*this);
	A.Scale3D *= InScale;
	return A;
}

FTransform FTransform::GetScaled(FVector InScale) const
{
	FTransform A(*this);
	A.Scale3D *= InScale;
	return A;
}

FVector FTransform::GetScaledAxis(EAxis::Type InAxis) const
{
	if (InAxis == EAxis::X)
	{
		return TransformVector(FVector(1.f, 0.f, 0.f));
	}
	if (InAxis == EAxis::Y)
	{
		return TransformVector(FVector(0.f, 1.f, 0.f));
	}
	return TransformVector(FVector(0.f, 0.f, 1.f));
}

FVector FTransform::GetUnitAxis(EAxis::Type InAxis) const
{
	if (InAxis == EAxis::X)
	{
		return TransformVectorNoScale(FVector(1.f, 0.f, 0.f));
	}
	if (InAxis == EAxis::Y)
	{
		return TransformVectorNoScale(FVector(0.f, 1.f, 0.f));
	}
	return TransformVectorNoScale(FVector(0.f, 0.f, 1.f));
}

void FTransform::Mirror(EAxis::Type MirrorAxis, EAxis::Type FlipAxis)
{
	// We do convert to Matrix for mirroring.
	FMatrix M = ToMatrixWithScale();
	M.Mirror(MirrorAxis, FlipAxis);
	SetFromMatrix(M);
}

FVector FTransform::GetSafeScaleReciprocal(const FVector& InScale, float Tolerance)
{
	FVector SafeReciprocalScale;
	SafeReciprocalScale.X = (FMath::Abs(InScale.X) <= Tolerance) ? 0.f : 1.f / InScale.X;
	SafeReciprocalScale.Y = (FMath::Abs(InScale.Y) <= Tolerance) ? 0.f : 1.f / InScale.Y;
	SafeReciprocalScale.Z = (FMath::Abs(InScale.Z) <= Tolerance) ? 0.f : 1.f / InScale.Z;
	return SafeReciprocalScale;
}
