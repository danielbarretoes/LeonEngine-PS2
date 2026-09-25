#include "Math/UnrealMath.h"

#include "Misc/Parse.h"

#include <cstring>

// Out-of-line math (UE: Math/UnrealMath.cpp): constants, vector / rotator / quaternion conversions, FMatrix and FPlane.

// Constants ----------------------------------------------------------------------------------------------------------

const FVector FVector::ZeroVector(0.0f, 0.0f, 0.0f);
const FVector FVector::OneVector(1.0f, 1.0f, 1.0f);
const FVector FVector::UpVector(0.0f, 0.0f, 1.0f);
const FVector FVector::DownVector(0.0f, 0.0f, -1.0f);
const FVector FVector::ForwardVector(1.0f, 0.0f, 0.0f);
const FVector FVector::BackwardVector(-1.0f, 0.0f, 0.0f);
const FVector FVector::RightVector(0.0f, 1.0f, 0.0f);
const FVector FVector::LeftVector(0.0f, -1.0f, 0.0f);
const FVector FVector::XAxisVector(1.0f, 0.0f, 0.0f);
const FVector FVector::YAxisVector(0.0f, 1.0f, 0.0f);
const FVector FVector::ZAxisVector(0.0f, 0.0f, 1.0f);

const FVector2D FVector2D::ZeroVector(0.0f, 0.0f);
const FVector2D FVector2D::UnitVector(1.0f, 1.0f);
const FVector2D FVector2D::Unit45Deg(UE_INV_SQRT_2, UE_INV_SQRT_2);

const FRotator FRotator::ZeroRotator(0.f, 0.f, 0.f);
const FQuat FQuat::Identity(0, 0, 0, 1);

const FMatrix FMatrix::Identity(FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 1, 0), FPlane(0, 0, 0, 1));

// FVector ------------------------------------------------------------------------------------------------------------

FVector::FVector(const FVector2D V, float InZ)
	: X(V.X)
	, Y(V.Y)
	, Z(InZ)
{
}

FVector::FVector(const FVector4& V)
	: X(V.X)
	, Y(V.Y)
	, Z(V.Z)
{
}

FVector::FVector(FIntVector InVector)
	: X(float(InVector.X))
	, Y(float(InVector.Y))
	, Z(float(InVector.Z))
{
}

FVector::FVector(FIntPoint A)
	: X(float(A.X))
	, Y(float(A.Y))
	, Z(0.f)
{
}

float FVector::GetComponentForAxis(EAxis::Type Axis) const
{
	switch (Axis)
	{
		case EAxis::X:
			return X;
		case EAxis::Y:
			return Y;
		case EAxis::Z:
			return Z;
		default:
			return 0.f;
	}
}

void FVector::SetComponentForAxis(EAxis::Type Axis, float Component)
{
	switch (Axis)
	{
		case EAxis::X:
			X = Component;
			break;
		case EAxis::Y:
			Y = Component;
			break;
		case EAxis::Z:
			Z = Component;
			break;
		default:
			break;
	}
}

void FVector::ToDirectionAndLength(FVector& OutDir, float& OutLength) const
{
	OutLength = Size();
	if (OutLength > SMALL_NUMBER)
	{
		const float OneOverLength = 1.0f / OutLength;
		OutDir = FVector(X * OneOverLength, Y * OneOverLength, Z * OneOverLength);
	}
	else
	{
		OutDir = FVector::ZeroVector;
	}
}

FVector FVector::GetClampedToSize(float Min, float Max) const
{
	float VecSize = Size();
	const FVector VecDir = (VecSize > SMALL_NUMBER) ? (*this / VecSize) : FVector::ZeroVector;

	VecSize = FMath::Clamp(VecSize, Min, Max);

	return VecSize * VecDir;
}

FVector FVector::GetClampedToSize2D(float Min, float Max) const
{
	float VecSize2D = Size2D();
	const FVector VecDir = (VecSize2D > SMALL_NUMBER) ? (*this / VecSize2D) : FVector::ZeroVector;

	VecSize2D = FMath::Clamp(VecSize2D, Min, Max);

	return FVector(VecSize2D * VecDir.X, VecSize2D * VecDir.Y, Z);
}

FVector FVector::GetClampedToMaxSize(float MaxSize) const
{
	if (MaxSize < KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float VSq = SizeSquared();
	if (VSq > FMath::Square(MaxSize))
	{
		const float Scale = MaxSize * FMath::InvSqrt(VSq);
		return FVector(X * Scale, Y * Scale, Z * Scale);
	}
	return *this;
}

FVector FVector::GetClampedToMaxSize2D(float MaxSize) const
{
	if (MaxSize < KINDA_SMALL_NUMBER)
	{
		return FVector(0.f, 0.f, Z);
	}

	const float VSq2D = SizeSquared2D();
	if (VSq2D > FMath::Square(MaxSize))
	{
		const float Scale = MaxSize * FMath::InvSqrt(VSq2D);
		return FVector(X * Scale, Y * Scale, Z);
	}
	return *this;
}

FVector FVector::BoundToCube(float Radius) const
{
	return FVector(
		FMath::Clamp(X, -Radius, Radius), FMath::Clamp(Y, -Radius, Radius), FMath::Clamp(Z, -Radius, Radius));
}

FVector FVector::BoundToBox(const FVector& Min, const FVector& Max) const
{
	return FVector(FMath::Clamp(X, Min.X, Max.X), FMath::Clamp(Y, Min.Y, Max.Y), FMath::Clamp(Z, Min.Z, Max.Z));
}

FVector FVector::Reciprocal() const
{
	FVector RecVector;
	RecVector.X = (X != 0.f) ? 1.f / X : BIG_NUMBER;
	RecVector.Y = (Y != 0.f) ? 1.f / Y : BIG_NUMBER;
	RecVector.Z = (Z != 0.f) ? 1.f / Z : BIG_NUMBER;
	return RecVector;
}

FVector FVector::MirrorByPlane(const FPlane& Plane) const
{
	return *this - Plane * (2.f * Plane.PlaneDot(*this));
}

FVector FVector::RotateAngleAxis(const float AngleDeg, const FVector& Axis) const
{
	float S, C;
	FMath::SinCos(&S, &C, FMath::DegreesToRadians(AngleDeg));

	const float XX = Axis.X * Axis.X;
	const float YY = Axis.Y * Axis.Y;
	const float ZZ = Axis.Z * Axis.Z;

	const float XY = Axis.X * Axis.Y;
	const float YZ = Axis.Y * Axis.Z;
	const float ZX = Axis.Z * Axis.X;

	const float XS = Axis.X * S;
	const float YS = Axis.Y * S;
	const float ZS = Axis.Z * S;

	const float OMC = 1.f - C;

	return FVector((OMC * XX + C) * X + (OMC * XY - ZS) * Y + (OMC * ZX + YS) * Z,
		(OMC * XY + ZS) * X + (OMC * YY + C) * Y + (OMC * YZ - XS) * Z,
		(OMC * ZX - YS) * X + (OMC * YZ + XS) * Y + (OMC * ZZ + C) * Z);
}

FRotator FVector::ToOrientationRotator() const
{
	FRotator R;

	// Find yaw.
	R.Yaw = FMath::Atan2(Y, X) * (180.f / PI);

	// Find pitch.
	R.Pitch = FMath::Atan2(Z, FMath::Sqrt(X * X + Y * Y)) * (180.f / PI);

	// Find roll.
	R.Roll = 0;

	return R;
}

FRotator FVector::Rotation() const
{
	return ToOrientationRotator();
}

FQuat FVector::ToOrientationQuat() const
{
	// Essentially an optimized Vector->Rotator->Quat made possible by knowing Roll == 0, and avoiding
	// radians->degrees->radians.
	const float YawRad = FMath::Atan2(Y, X);
	const float PitchRad = FMath::Atan2(Z, FMath::Sqrt(X * X + Y * Y));

	float SP, SY;
	float CP, CY;
	FMath::SinCos(&SP, &CP, PitchRad * 0.5f);
	FMath::SinCos(&SY, &CY, YawRad * 0.5f);

	FQuat RotationQuat;
	RotationQuat.X = SP * SY;
	RotationQuat.Y = -SP * CY;
	RotationQuat.Z = CP * SY;
	RotationQuat.W = CP * CY;
	return RotationQuat;
}

float FVector::HeadingAngle() const
{
	// Project Dir into Z plane.
	FVector PlaneDir = *this;
	PlaneDir.Z = 0.f;
	PlaneDir = PlaneDir.GetSafeNormal();

	float Angle = FMath::Acos(PlaneDir.X);
	if (PlaneDir.Y < 0.0f)
	{
		Angle *= -1.0f;
	}
	return Angle;
}

void FVector::FindBestAxisVectors(FVector& Axis1, FVector& Axis2) const
{
	const float NX = FMath::Abs(X);
	const float NY = FMath::Abs(Y);
	const float NZ = FMath::Abs(Z);

	// Find best basis vectors.
	if (NZ > NX && NZ > NY)
	{
		Axis1 = FVector(1, 0, 0);
	}
	else
	{
		Axis1 = FVector(0, 0, 1);
	}

	Axis1 = (Axis1 - *this * (Axis1 | *this)).GetSafeNormal();
	Axis2 = Axis1 ^ *this;
}

void FVector::UnwindEuler()
{
	X = FMath::UnwindDegrees(X);
	Y = FMath::UnwindDegrees(Y);
	Z = FMath::UnwindDegrees(Z);
}

FString FVector::ToString() const
{
	return FString::Printf("X=%3.3f Y=%3.3f Z=%3.3f", double(X), double(Y), double(Z));
}

FString FVector::ToCompactString() const
{
	if (IsNearlyZero())
	{
		return FString("V(0)");
	}

	FString ReturnString("V(");
	bool bIsEmptyString = true;
	if (!FMath::IsNearlyZero(X))
	{
		ReturnString += FString::Printf("X=%.2f", double(X));
		bIsEmptyString = false;
	}
	if (!FMath::IsNearlyZero(Y))
	{
		if (!bIsEmptyString)
		{
			ReturnString += FString(", ");
		}
		ReturnString += FString::Printf("Y=%.2f", double(Y));
		bIsEmptyString = false;
	}
	if (!FMath::IsNearlyZero(Z))
	{
		if (!bIsEmptyString)
		{
			ReturnString += FString(", ");
		}
		ReturnString += FString::Printf("Z=%.2f", double(Z));
	}
	ReturnString += FString(")");
	return ReturnString;
}

bool FVector::InitFromString(const FString& InSourceString)
{
	X = Y = Z = 0;

	// The initialization is only successful if the X, Y, and Z values can all be parsed from the string.
	return FParse::Value(*InSourceString, "X=", X) && FParse::Value(*InSourceString, "Y=", Y) &&
		FParse::Value(*InSourceString, "Z=", Z);
}

float FVector::CosineAngle2D(FVector A, FVector B)
{
	A.Z = 0.0f;
	B.Z = 0.0f;
	A.Normalize();
	B.Normalize();
	return A | B;
}

bool FVector::PointsAreSame(const FVector& P, const FVector& Q)
{
	float Temp = P.X - Q.X;
	if ((Temp > -THRESH_POINTS_ARE_SAME) && (Temp < THRESH_POINTS_ARE_SAME))
	{
		Temp = P.Y - Q.Y;
		if ((Temp > -THRESH_POINTS_ARE_SAME) && (Temp < THRESH_POINTS_ARE_SAME))
		{
			Temp = P.Z - Q.Z;
			if ((Temp > -THRESH_POINTS_ARE_SAME) && (Temp < THRESH_POINTS_ARE_SAME))
			{
				return true;
			}
		}
	}
	return false;
}

bool FVector::PointsAreNear(const FVector& Point1, const FVector& Point2, float Dist)
{
	if (FMath::Abs(Point1.X - Point2.X) >= Dist)
	{
		return false;
	}
	if (FMath::Abs(Point1.Y - Point2.Y) >= Dist)
	{
		return false;
	}
	if (FMath::Abs(Point1.Z - Point2.Z) >= Dist)
	{
		return false;
	}
	return true;
}

// FVector2D / FVector4 / FIntPoint / FIntVector ----------------------------------------------------------------------

FVector2D::FVector2D(FIntPoint InPos)
	: X(float(InPos.X))
	, Y(float(InPos.Y))
{
}

FVector2D::FVector2D(const FVector& V)
	: X(V.X)
	, Y(V.Y)
{
}

FVector2D FVector2D::GetRotated(float AngleDeg) const
{
	// Based on FVector::RotateAngleAxis with Axis(0,0,1).
	float S, C;
	FMath::SinCos(&S, &C, FMath::DegreesToRadians(AngleDeg));

	return FVector2D(C * X - S * Y, S * X + C * Y);
}

FString FVector2D::ToString() const
{
	return FString::Printf("X=%3.3f Y=%3.3f", double(X), double(Y));
}

bool FVector2D::InitFromString(const FString& InSourceString)
{
	X = Y = 0;

	// The initialization is only successful if the X and Y values can all be parsed from the string.
	return FParse::Value(*InSourceString, "X=", X) && FParse::Value(*InSourceString, "Y=", Y);
}

FRotator FVector4::ToOrientationRotator() const
{
	return FVector(*this).ToOrientationRotator();
}

FQuat FVector4::ToOrientationQuat() const
{
	return FVector(*this).ToOrientationQuat();
}

FString FVector4::ToString() const
{
	return FString::Printf("X=%3.3f Y=%3.3f Z=%3.3f W=%3.3f", double(X), double(Y), double(Z), double(W));
}

bool FVector4::InitFromString(const FString& InSourceString)
{
	X = Y = Z = 0;
	W = 1.0f;

	// The initialization is only successful if the X, Y, and Z values can all be parsed from the string.
	const bool bSuccessful = FParse::Value(*InSourceString, "X=", X) && FParse::Value(*InSourceString, "Y=", Y) &&
		FParse::Value(*InSourceString, "Z=", Z);

	// W is optional, so don't factor in its presence (or lack thereof) in determining initialization success.
	FParse::Value(*InSourceString, "W=", W);

	return bSuccessful;
}

bool FIntPoint::InitFromString(const FString& InSourceString)
{
	X = Y = 0;

	// The initialization is only successful if the X and Y values can all be parsed from the string.
	return FParse::Value(*InSourceString, "X=", X) && FParse::Value(*InSourceString, "Y=", Y);
}

FIntVector::FIntVector(const FVector& InVector)
	: X(FMath::TruncToInt(InVector.X))
	, Y(FMath::TruncToInt(InVector.Y))
	, Z(FMath::TruncToInt(InVector.Z))
{
}

// FRotator -----------------------------------------------------------------------------------------------------------

FRotator::FRotator(const FQuat& Quat)
{
	*this = Quat.Rotator();
}

FRotator FRotator::GetInverse() const
{
	return Quaternion().Inverse().Rotator();
}

FRotator FRotator::GridSnap(const FRotator& RotGrid) const
{
	return FRotator(
		FMath::GridSnap(Pitch, RotGrid.Pitch), FMath::GridSnap(Yaw, RotGrid.Yaw), FMath::GridSnap(Roll, RotGrid.Roll));
}

FVector FRotator::Vector() const
{
	// Remove winding and clamp to [-360, 360].
	const float PitchNoWinding = FMath::Fmod(Pitch, 360.0f);
	const float YawNoWinding = FMath::Fmod(Yaw, 360.0f);

	float CP, SP, CY, SY;
	FMath::SinCos(&SP, &CP, FMath::DegreesToRadians(PitchNoWinding));
	FMath::SinCos(&SY, &CY, FMath::DegreesToRadians(YawNoWinding));
	return FVector(CP * CY, CP * SY, SP);
}

FQuat FRotator::Quaternion() const
{
	const float RadsDividedBy2 = (PI / 180.f) / 2.f;
	float SP, SY, SR;
	float CP, CY, CR;

	const float PitchNoWinding = FMath::Fmod(Pitch, 360.0f);
	const float YawNoWinding = FMath::Fmod(Yaw, 360.0f);
	const float RollNoWinding = FMath::Fmod(Roll, 360.0f);

	FMath::SinCos(&SP, &CP, PitchNoWinding * RadsDividedBy2);
	FMath::SinCos(&SY, &CY, YawNoWinding * RadsDividedBy2);
	FMath::SinCos(&SR, &CR, RollNoWinding * RadsDividedBy2);

	FQuat RotationQuat;
	RotationQuat.X = CR * SP * SY - SR * CP * CY;
	RotationQuat.Y = -CR * SP * CY - SR * CP * SY;
	RotationQuat.Z = CR * CP * SY - SR * SP * CY;
	RotationQuat.W = CR * CP * CY + SR * SP * SY;
	return RotationQuat;
}

FVector FRotator::Euler() const
{
	return FVector(Roll, Pitch, Yaw);
}

FRotator FRotator::MakeFromEuler(const FVector& Euler)
{
	return FRotator(Euler.Y, Euler.Z, Euler.X);
}

FVector FRotator::RotateVector(const FVector& V) const
{
	return FVector(FRotationMatrix(*this).TransformVector(V));
}

FVector FRotator::UnrotateVector(const FVector& V) const
{
	return FVector(FRotationMatrix(*this).GetTransposed().TransformVector(V));
}

FRotator FRotator::GetEquivalentRotator() const
{
	return FRotator(180.0f - Pitch, Yaw + 180.0f, Roll + 180.0f);
}

void FRotator::SetClosestToMe(FRotator& MakeClosest) const
{
	const FRotator OtherChoice = MakeClosest.GetEquivalentRotator();
	const float FirstDiff = GetManhattanDistance(MakeClosest);
	const float SecondDiff = GetManhattanDistance(OtherChoice);
	if (SecondDiff < FirstDiff)
	{
		MakeClosest = OtherChoice;
	}
}

float FRotator::GetManhattanDistance(const FRotator& Rotator) const
{
	return FMath::Abs(FRotator::NormalizeAxis(Yaw - Rotator.Yaw)) +
		FMath::Abs(FRotator::NormalizeAxis(Pitch - Rotator.Pitch)) +
		FMath::Abs(FRotator::NormalizeAxis(Roll - Rotator.Roll));
}

FString FRotator::ToString() const
{
	return FString::Printf("P=%f Y=%f R=%f", double(Pitch), double(Yaw), double(Roll));
}

FString FRotator::ToCompactString() const
{
	if (IsNearlyZero())
	{
		return FString("R(0)");
	}

	FString ReturnString("R(");
	bool bIsEmptyString = true;
	if (!FMath::IsNearlyZero(Pitch))
	{
		ReturnString += FString::Printf("P=%.2f", double(Pitch));
		bIsEmptyString = false;
	}
	if (!FMath::IsNearlyZero(Yaw))
	{
		if (!bIsEmptyString)
		{
			ReturnString += FString(", ");
		}
		ReturnString += FString::Printf("Y=%.2f", double(Yaw));
		bIsEmptyString = false;
	}
	if (!FMath::IsNearlyZero(Roll))
	{
		if (!bIsEmptyString)
		{
			ReturnString += FString(", ");
		}
		ReturnString += FString::Printf("R=%.2f", double(Roll));
	}
	ReturnString += FString(")");
	return ReturnString;
}

bool FRotator::InitFromString(const FString& InSourceString)
{
	Pitch = Yaw = Roll = 0;

	// The initialization is only successful if the X, Y, and Z values can all be parsed from the string.
	return FParse::Value(*InSourceString, "P=", Pitch) && FParse::Value(*InSourceString, "Y=", Yaw) &&
		FParse::Value(*InSourceString, "R=", Roll);
}

// FQuat --------------------------------------------------------------------------------------------------------------

FQuat::FQuat(const FRotator& R)
{
	*this = R.Quaternion();
}

FQuat::FQuat(const FMatrix& M)
{
	// If Matrix is NULL, return Identity quaternion. If any of them is 0, you won't be able to construct rotation.
	if (M.GetScaledAxis(EAxis::X).IsNearlyZero() || M.GetScaledAxis(EAxis::Y).IsNearlyZero() ||
		M.GetScaledAxis(EAxis::Z).IsNearlyZero())
	{
		*this = FQuat::Identity;
		return;
	}

	float S;

	// Check diagonal (trace).
	const float Tr = M.M[0][0] + M.M[1][1] + M.M[2][2];

	if (Tr > 0.0f)
	{
		const float InvS = FMath::InvSqrt(Tr + 1.f);
		W = 0.5f * (1.f / InvS);
		S = 0.5f * InvS;

		X = (M.M[1][2] - M.M[2][1]) * S;
		Y = (M.M[2][0] - M.M[0][2]) * S;
		Z = (M.M[0][1] - M.M[1][0]) * S;
	}
	else
	{
		// Diagonal is negative.
		int32 I = 0;

		if (M.M[1][1] > M.M[0][0])
		{
			I = 1;
		}

		if (M.M[2][2] > M.M[I][I])
		{
			I = 2;
		}

		static const int32 Next[3] = {1, 2, 0};
		const int32 J = Next[I];
		const int32 K = Next[J];

		S = M.M[I][I] - M.M[J][J] - M.M[K][K] + 1.0f;

		const float InvS = FMath::InvSqrt(S);

		float Qt[4];
		Qt[I] = 0.5f * (1.f / InvS);

		S = 0.5f * InvS;

		Qt[3] = (M.M[J][K] - M.M[K][J]) * S;
		Qt[J] = (M.M[I][J] + M.M[J][I]) * S;
		Qt[K] = (M.M[I][K] + M.M[K][I]) * S;

		X = Qt[0];
		Y = Qt[1];
		Z = Qt[2];
		W = Qt[3];
	}
}

FQuat::FQuat(FVector Axis, float AngleRad)
{
	const float HalfA = 0.5f * AngleRad;
	float S, C;
	FMath::SinCos(&S, &C, HalfA);

	X = S * Axis.X;
	Y = S * Axis.Y;
	Z = S * Axis.Z;
	W = C;
}

void FQuat::ToAxisAndAngle(FVector& Axis, float& Angle) const
{
	Angle = GetAngle();
	Axis = GetRotationAxis();
}

FVector FQuat::GetRotationAxis() const
{
	// Ensure we never try to sqrt a neg number.
	const float S = FMath::Sqrt(FMath::Max(1.f - (W * W), 0.f));

	if (S >= 0.0001f)
	{
		return FVector(X / S, Y / S, Z / S);
	}

	return FVector(1.f, 0.f, 0.f);
}

FVector FQuat::Euler() const
{
	return Rotator().Euler();
}

FQuat FQuat::MakeFromEuler(const FVector& Euler)
{
	return FRotator::MakeFromEuler(Euler).Quaternion();
}

FRotator FQuat::Rotator() const
{
	const float SingularityTest = Z * X - W * Y;
	const float YawY = 2.f * (W * Z + X * Y);
	const float YawX = (1.f - 2.f * (FMath::Square(Y) + FMath::Square(Z)));

	// Reference:
	// http://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/
	// This value was found from experience, the above websites recommend different values but that isn't the case
	// for us, so I went through different testing, and finally found the case where both of world lives happily.
	const float SingularityThreshold = 0.4999995f;
	const float RadToDeg = (180.f) / PI;
	FRotator RotatorFromQuat;

	if (SingularityTest < -SingularityThreshold)
	{
		RotatorFromQuat.Pitch = -90.f;
		RotatorFromQuat.Yaw = FMath::Atan2(YawY, YawX) * RadToDeg;
		RotatorFromQuat.Roll = FRotator::NormalizeAxis(-RotatorFromQuat.Yaw - (2.f * FMath::Atan2(X, W) * RadToDeg));
	}
	else if (SingularityTest > SingularityThreshold)
	{
		RotatorFromQuat.Pitch = 90.f;
		RotatorFromQuat.Yaw = FMath::Atan2(YawY, YawX) * RadToDeg;
		RotatorFromQuat.Roll = FRotator::NormalizeAxis(RotatorFromQuat.Yaw - (2.f * FMath::Atan2(X, W) * RadToDeg));
	}
	else
	{
		RotatorFromQuat.Pitch = FMath::FastAsin(2.f * (SingularityTest)) * RadToDeg;
		RotatorFromQuat.Yaw = FMath::Atan2(YawY, YawX) * RadToDeg;
		RotatorFromQuat.Roll =
			FMath::Atan2(-2.f * (W * X + Y * Z), (1.f - 2.f * (FMath::Square(X) + FMath::Square(Y)))) * RadToDeg;
	}

	return RotatorFromQuat;
}

FString FQuat::ToString() const
{
	return FString::Printf("X=%.9f Y=%.9f Z=%.9f W=%.9f", double(X), double(Y), double(Z), double(W));
}

bool FQuat::InitFromString(const FString& InSourceString)
{
	X = Y = Z = 0.f;
	W = 1.f;

	return FParse::Value(*InSourceString, "X=", X) && FParse::Value(*InSourceString, "Y=", Y) &&
		FParse::Value(*InSourceString, "Z=", Z) && FParse::Value(*InSourceString, "W=", W);
}

namespace
{
	FQuat FindBetweenHelper(const FVector& A, const FVector& B, float NormAB)
	{
		float W = NormAB + FVector::DotProduct(A, B);
		FQuat Result;

		if (W >= 1e-6f * NormAB)
		{
			// Axis = FVector::CrossProduct(A, B);
			Result = FQuat(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X, W);
		}
		else
		{
			// A and B point in opposite directions.
			W = 0.f;
			Result = FMath::Abs(A.X) > FMath::Abs(A.Y) ? FQuat(-A.Z, 0.f, A.X, W) : FQuat(0.f, -A.Z, A.Y, W);
		}

		Result.Normalize();
		return Result;
	}
} // namespace

FQuat FQuat::FindBetweenNormals(const FVector& Normal1, const FVector& Normal2)
{
	const float NormAB = 1.f;
	return FindBetweenHelper(Normal1, Normal2, NormAB);
}

FQuat FQuat::FindBetweenVectors(const FVector& Vector1, const FVector& Vector2)
{
	const float NormAB = FMath::Sqrt(Vector1.SizeSquared() * Vector2.SizeSquared());
	return FindBetweenHelper(Vector1, Vector2, NormAB);
}

FQuat FQuat::Slerp_NotNormalized(const FQuat& Quat1, const FQuat& Quat2, float Slerp)
{
	// Get cosine of angle between quats.
	const float RawCosom = Quat1.X * Quat2.X + Quat1.Y * Quat2.Y + Quat1.Z * Quat2.Z + Quat1.W * Quat2.W;
	// Unaligned quats - compensate, results in taking shorter route.
	const float Cosom = FMath::FloatSelect(RawCosom, RawCosom, -RawCosom);

	float Scale0, Scale1;

	if (Cosom < 0.9999f)
	{
		const float Omega = FMath::Acos(Cosom);
		const float InvSin = 1.f / FMath::Sin(Omega);
		Scale0 = FMath::Sin((1.f - Slerp) * Omega) * InvSin;
		Scale1 = FMath::Sin(Slerp * Omega) * InvSin;
	}
	else
	{
		// Use linear interpolation.
		Scale0 = 1.0f - Slerp;
		Scale1 = Slerp;
	}

	// In keeping with our flipped Cosom:
	Scale1 = FMath::FloatSelect(RawCosom, Scale1, -Scale1);

	FQuat Result;
	Result.X = Scale0 * Quat1.X + Scale1 * Quat2.X;
	Result.Y = Scale0 * Quat1.Y + Scale1 * Quat2.Y;
	Result.Z = Scale0 * Quat1.Z + Scale1 * Quat2.Z;
	Result.W = Scale0 * Quat1.W + Scale1 * Quat2.W;
	return Result;
}

// FMatrix ------------------------------------------------------------------------------------------------------------

FMatrix::FMatrix(const FPlane& InX, const FPlane& InY, const FPlane& InZ, const FPlane& InW)
{
	M[0][0] = InX.X;
	M[0][1] = InX.Y;
	M[0][2] = InX.Z;
	M[0][3] = InX.W;
	M[1][0] = InY.X;
	M[1][1] = InY.Y;
	M[1][2] = InY.Z;
	M[1][3] = InY.W;
	M[2][0] = InZ.X;
	M[2][1] = InZ.Y;
	M[2][2] = InZ.Z;
	M[2][3] = InZ.W;
	M[3][0] = InW.X;
	M[3][1] = InW.Y;
	M[3][2] = InW.Z;
	M[3][3] = InW.W;
}

FMatrix::FMatrix(const FVector& InX, const FVector& InY, const FVector& InZ, const FVector& InW)
{
	M[0][0] = InX.X;
	M[0][1] = InX.Y;
	M[0][2] = InX.Z;
	M[0][3] = 0.0f;
	M[1][0] = InY.X;
	M[1][1] = InY.Y;
	M[1][2] = InY.Z;
	M[1][3] = 0.0f;
	M[2][0] = InZ.X;
	M[2][1] = InZ.Y;
	M[2][2] = InZ.Z;
	M[2][3] = 0.0f;
	M[3][0] = InW.X;
	M[3][1] = InW.Y;
	M[3][2] = InW.Z;
	M[3][3] = 1.0f;
}

void FMatrix::SetIdentity()
{
	*this = Identity;
}

FMatrix FMatrix::operator*(const FMatrix& Other) const
{
	FMatrix Result;
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			Result.M[Row][Col] = M[Row][0] * Other.M[0][Col] + M[Row][1] * Other.M[1][Col] +
				M[Row][2] * Other.M[2][Col] + M[Row][3] * Other.M[3][Col];
		}
	}
	return Result;
}

void FMatrix::operator*=(const FMatrix& Other)
{
	*this = *this * Other;
}

FMatrix FMatrix::operator+(const FMatrix& Other) const
{
	FMatrix ResultMat;
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			ResultMat.M[Row][Col] = M[Row][Col] + Other.M[Row][Col];
		}
	}
	return ResultMat;
}

void FMatrix::operator+=(const FMatrix& Other)
{
	*this = *this + Other;
}

FMatrix FMatrix::operator*(float Other) const
{
	FMatrix ResultMat;
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			ResultMat.M[Row][Col] = M[Row][Col] * Other;
		}
	}
	return ResultMat;
}

void FMatrix::operator*=(float Other)
{
	*this = *this * Other;
}

bool FMatrix::operator==(const FMatrix& Other) const
{
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			if (M[Row][Col] != Other.M[Row][Col])
			{
				return false;
			}
		}
	}
	return true;
}

bool FMatrix::Equals(const FMatrix& Other, float Tolerance) const
{
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			if (FMath::Abs(M[Row][Col] - Other.M[Row][Col]) > Tolerance)
			{
				return false;
			}
		}
	}
	return true;
}

FVector4 FMatrix::TransformFVector4(const FVector4& P) const
{
	FVector4 Result;
	Result.X = P.X * M[0][0] + P.Y * M[1][0] + P.Z * M[2][0] + P.W * M[3][0];
	Result.Y = P.X * M[0][1] + P.Y * M[1][1] + P.Z * M[2][1] + P.W * M[3][1];
	Result.Z = P.X * M[0][2] + P.Y * M[1][2] + P.Z * M[2][2] + P.W * M[3][2];
	Result.W = P.X * M[0][3] + P.Y * M[1][3] + P.Z * M[2][3] + P.W * M[3][3];
	return Result;
}

FVector4 FMatrix::TransformPosition(const FVector& V) const
{
	return TransformFVector4(FVector4(V.X, V.Y, V.Z, 1.0f));
}

FVector FMatrix::InverseTransformPosition(const FVector& V) const
{
	const FMatrix InvSelf = InverseFast();
	return FVector(InvSelf.TransformPosition(V));
}

FVector4 FMatrix::TransformVector(const FVector& V) const
{
	return TransformFVector4(FVector4(V.X, V.Y, V.Z, 0.0f));
}

FVector FMatrix::InverseTransformVector(const FVector& V) const
{
	const FMatrix InvSelf = InverseFast();
	return FVector(InvSelf.TransformVector(V));
}

FMatrix FMatrix::GetTransposed() const
{
	FMatrix Result;
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			Result.M[Row][Col] = M[Col][Row];
		}
	}
	return Result;
}

float FMatrix::Determinant() const
{
	return M[0][0] *
		(M[1][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) - M[2][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) +
			M[3][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2])) -
		M[1][0] *
		(M[0][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) - M[2][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
			M[3][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2])) +
		M[2][0] *
		(M[0][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) - M[1][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
			M[3][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])) -
		M[3][0] *
		(M[0][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2]) - M[1][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2]) +
			M[2][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2]));
}

float FMatrix::RotDeterminant() const
{
	return M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) - M[1][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1]) +
		M[2][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1]);
}

FMatrix FMatrix::InverseFast() const
{
	// Laplace expansion by 2x2 minors (the UE generic VectorMatrixInverse computes the same).
	const float A00 = M[0][0], A01 = M[0][1], A02 = M[0][2], A03 = M[0][3];
	const float A10 = M[1][0], A11 = M[1][1], A12 = M[1][2], A13 = M[1][3];
	const float A20 = M[2][0], A21 = M[2][1], A22 = M[2][2], A23 = M[2][3];
	const float A30 = M[3][0], A31 = M[3][1], A32 = M[3][2], A33 = M[3][3];

	const float S0 = A00 * A11 - A10 * A01;
	const float S1 = A00 * A12 - A10 * A02;
	const float S2 = A00 * A13 - A10 * A03;
	const float S3 = A01 * A12 - A11 * A02;
	const float S4 = A01 * A13 - A11 * A03;
	const float S5 = A02 * A13 - A12 * A03;

	const float C5 = A22 * A33 - A32 * A23;
	const float C4 = A21 * A33 - A31 * A23;
	const float C3 = A21 * A32 - A31 * A22;
	const float C2 = A20 * A33 - A30 * A23;
	const float C1 = A20 * A32 - A30 * A22;
	const float C0 = A20 * A31 - A30 * A21;

	const float Det = S0 * C5 - S1 * C4 + S2 * C3 + S3 * C2 - S4 * C1 + S5 * C0;
	const float InvDet = 1.0f / Det;

	FMatrix Result;
	Result.M[0][0] = (A11 * C5 - A12 * C4 + A13 * C3) * InvDet;
	Result.M[0][1] = (-A01 * C5 + A02 * C4 - A03 * C3) * InvDet;
	Result.M[0][2] = (A31 * S5 - A32 * S4 + A33 * S3) * InvDet;
	Result.M[0][3] = (-A21 * S5 + A22 * S4 - A23 * S3) * InvDet;

	Result.M[1][0] = (-A10 * C5 + A12 * C2 - A13 * C1) * InvDet;
	Result.M[1][1] = (A00 * C5 - A02 * C2 + A03 * C1) * InvDet;
	Result.M[1][2] = (-A30 * S5 + A32 * S2 - A33 * S1) * InvDet;
	Result.M[1][3] = (A20 * S5 - A22 * S2 + A23 * S1) * InvDet;

	Result.M[2][0] = (A10 * C4 - A11 * C2 + A13 * C0) * InvDet;
	Result.M[2][1] = (-A00 * C4 + A01 * C2 - A03 * C0) * InvDet;
	Result.M[2][2] = (A30 * S4 - A31 * S2 + A33 * S0) * InvDet;
	Result.M[2][3] = (-A20 * S4 + A21 * S2 - A23 * S0) * InvDet;

	Result.M[3][0] = (-A10 * C3 + A11 * C1 - A12 * C0) * InvDet;
	Result.M[3][1] = (A00 * C3 - A01 * C1 + A02 * C0) * InvDet;
	Result.M[3][2] = (-A30 * S3 + A31 * S1 - A32 * S0) * InvDet;
	Result.M[3][3] = (A20 * S3 - A21 * S1 + A22 * S0) * InvDet;
	return Result;
}

FMatrix FMatrix::Inverse() const
{
	// Check for zero scale matrix to invert.
	if (GetScaledAxis(EAxis::X).IsNearlyZero(SMALL_NUMBER) && GetScaledAxis(EAxis::Y).IsNearlyZero(SMALL_NUMBER) &&
		GetScaledAxis(EAxis::Z).IsNearlyZero(SMALL_NUMBER))
	{
		// Just set to zero - avoids unsafe inverse of zero and duplicates what QNANs were resulting in before
		// (scaling away all children).
		return FMatrix::Identity;
	}

	if (Determinant() == 0.0f)
	{
		return FMatrix::Identity;
	}

	return InverseFast();
}

FMatrix FMatrix::TransposeAdjoint() const
{
	FMatrix TA;

	TA.M[0][0] = M[1][1] * M[2][2] - M[1][2] * M[2][1];
	TA.M[0][1] = M[1][2] * M[2][0] - M[1][0] * M[2][2];
	TA.M[0][2] = M[1][0] * M[2][1] - M[1][1] * M[2][0];
	TA.M[0][3] = 0.f;

	TA.M[1][0] = M[2][1] * M[0][2] - M[2][2] * M[0][1];
	TA.M[1][1] = M[2][2] * M[0][0] - M[2][0] * M[0][2];
	TA.M[1][2] = M[2][0] * M[0][1] - M[2][1] * M[0][0];
	TA.M[1][3] = 0.f;

	TA.M[2][0] = M[0][1] * M[1][2] - M[0][2] * M[1][1];
	TA.M[2][1] = M[0][2] * M[1][0] - M[0][0] * M[1][2];
	TA.M[2][2] = M[0][0] * M[1][1] - M[0][1] * M[1][0];
	TA.M[2][3] = 0.f;

	TA.M[3][0] = 0.f;
	TA.M[3][1] = 0.f;
	TA.M[3][2] = 0.f;
	TA.M[3][3] = 1.f;

	return TA;
}

void FMatrix::RemoveScaling(float Tolerance)
{
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const float SquareSum = (M[Row][0] * M[Row][0]) + (M[Row][1] * M[Row][1]) + (M[Row][2] * M[Row][2]);
		const float Scale = FMath::FloatSelect(SquareSum - Tolerance, FMath::InvSqrt(SquareSum), 1.0f);
		M[Row][0] *= Scale;
		M[Row][1] *= Scale;
		M[Row][2] *= Scale;
	}
}

FMatrix FMatrix::GetMatrixWithoutScale(float Tolerance) const
{
	FMatrix Result = *this;
	Result.RemoveScaling(Tolerance);
	return Result;
}

FVector FMatrix::ExtractScaling(float Tolerance)
{
	FVector Scale3D(0, 0, 0);

	// For each row, find magnitude, and if its non-zero re-scale so its unit length.
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const float SquareSum = (M[Row][0] * M[Row][0]) + (M[Row][1] * M[Row][1]) + (M[Row][2] * M[Row][2]);
		if (SquareSum > Tolerance)
		{
			const float Scale = FMath::Sqrt(SquareSum);
			Scale3D[Row] = Scale;
			const float InvScale = 1.f / Scale;
			M[Row][0] *= InvScale;
			M[Row][1] *= InvScale;
			M[Row][2] *= InvScale;
		}
		else
		{
			Scale3D[Row] = 0;
		}
	}

	return Scale3D;
}

FVector FMatrix::GetScaleVector(float Tolerance) const
{
	FVector Scale3D(1, 1, 1);

	// For each row, find magnitude, and if its non-zero re-scale so its unit length.
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const float SquareSum = (M[Row][0] * M[Row][0]) + (M[Row][1] * M[Row][1]) + (M[Row][2] * M[Row][2]);
		Scale3D[Row] = (SquareSum > Tolerance) ? FMath::Sqrt(SquareSum) : 0.f;
	}

	return Scale3D;
}

FMatrix FMatrix::RemoveTranslation() const
{
	FMatrix Result = *this;
	Result.M[3][0] = 0.0f;
	Result.M[3][1] = 0.0f;
	Result.M[3][2] = 0.0f;
	return Result;
}

FMatrix FMatrix::ConcatTranslation(const FVector& Translation) const
{
	FMatrix Result = *this;
	Result.M[3][0] += Translation.X;
	Result.M[3][1] += Translation.Y;
	Result.M[3][2] += Translation.Z;
	return Result;
}

bool FMatrix::ContainsNaN() const
{
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			if (!FMath::IsFinite(M[Row][Col]))
			{
				return true;
			}
		}
	}
	return false;
}

void FMatrix::ScaleTranslation(const FVector& Scale3D)
{
	M[3][0] *= Scale3D.X;
	M[3][1] *= Scale3D.Y;
	M[3][2] *= Scale3D.Z;
}

float FMatrix::GetMinimumAxisScale() const
{
	const float MinRowScaleSquared = FMath::Min3(GetScaledAxis(EAxis::X).SizeSquared(),
		GetScaledAxis(EAxis::Y).SizeSquared(), GetScaledAxis(EAxis::Z).SizeSquared());
	return FMath::Sqrt(MinRowScaleSquared);
}

float FMatrix::GetMaximumAxisScale() const
{
	const float MaxRowScaleSquared = FMath::Max3(GetScaledAxis(EAxis::X).SizeSquared(),
		GetScaledAxis(EAxis::Y).SizeSquared(), GetScaledAxis(EAxis::Z).SizeSquared());
	return FMath::Sqrt(MaxRowScaleSquared);
}

FMatrix FMatrix::ApplyScale(float Scale) const
{
	const FMatrix ScaleMatrix(FPlane(Scale, 0.0f, 0.0f, 0.0f), FPlane(0.0f, Scale, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, Scale, 0.0f), FPlane(0.0f, 0.0f, 0.0f, 1.0f));
	return ScaleMatrix * (*this);
}

FVector FMatrix::GetScaledAxis(EAxis::Type InAxis) const
{
	switch (InAxis)
	{
		case EAxis::X:
			return FVector(M[0][0], M[0][1], M[0][2]);

		case EAxis::Y:
			return FVector(M[1][0], M[1][1], M[1][2]);

		case EAxis::Z:
			return FVector(M[2][0], M[2][1], M[2][2]);

		default:
			return FVector::ZeroVector;
	}
}

void FMatrix::GetScaledAxes(FVector& X, FVector& Y, FVector& Z) const
{
	X.X = M[0][0];
	X.Y = M[0][1];
	X.Z = M[0][2];
	Y.X = M[1][0];
	Y.Y = M[1][1];
	Y.Z = M[1][2];
	Z.X = M[2][0];
	Z.Y = M[2][1];
	Z.Z = M[2][2];
}

FVector FMatrix::GetUnitAxis(EAxis::Type InAxis) const
{
	return GetScaledAxis(InAxis).GetSafeNormal();
}

void FMatrix::GetUnitAxes(FVector& X, FVector& Y, FVector& Z) const
{
	GetScaledAxes(X, Y, Z);
	X.Normalize();
	Y.Normalize();
	Z.Normalize();
}

void FMatrix::SetAxis(int32 i, const FVector& Axis)
{
	checkSlow(i >= 0 && i <= 2);
	M[i][0] = Axis.X;
	M[i][1] = Axis.Y;
	M[i][2] = Axis.Z;
}

void FMatrix::SetOrigin(const FVector& NewOrigin)
{
	M[3][0] = NewOrigin.X;
	M[3][1] = NewOrigin.Y;
	M[3][2] = NewOrigin.Z;
}

void FMatrix::SetAxes(FVector* Axis0, FVector* Axis1, FVector* Axis2, FVector* Origin)
{
	if (Axis0 != nullptr)
	{
		SetAxis(0, *Axis0);
	}
	if (Axis1 != nullptr)
	{
		SetAxis(1, *Axis1);
	}
	if (Axis2 != nullptr)
	{
		SetAxis(2, *Axis2);
	}
	if (Origin != nullptr)
	{
		SetOrigin(*Origin);
	}
}

FVector FMatrix::GetColumn(int32 i) const
{
	checkSlow(i >= 0 && i <= 3);
	return FVector(M[0][i], M[1][i], M[2][i]);
}

void FMatrix::SetColumn(int32 i, FVector Value)
{
	checkSlow(i >= 0 && i <= 3);
	M[0][i] = Value.X;
	M[1][i] = Value.Y;
	M[2][i] = Value.Z;
}

FRotator FMatrix::Rotator() const
{
	const FVector XAxis = GetScaledAxis(EAxis::X);
	const FVector YAxis = GetScaledAxis(EAxis::Y);
	const FVector ZAxis = GetScaledAxis(EAxis::Z);

	FRotator Rot =
		FRotator(FMath::Atan2(XAxis.Z, FMath::Sqrt(FMath::Square(XAxis.X) + FMath::Square(XAxis.Y))) * 180.f / PI,
			FMath::Atan2(XAxis.Y, XAxis.X) * 180.f / PI, 0);

	const FVector SYAxis = FRotationMatrix(Rot).GetScaledAxis(EAxis::Y);
	Rot.Roll = FMath::Atan2(ZAxis | SYAxis, YAxis | SYAxis) * 180.f / PI;

	return Rot;
}

FQuat FMatrix::ToQuat() const
{
	return FQuat(*this);
}

namespace
{
	bool MakeFrustumPlane(float A, float B, float C, float D, FPlane& OutPlane)
	{
		const float LengthSquared = A * A + B * B + C * C;
		if (LengthSquared > DELTA * DELTA)
		{
			const float InvLength = FMath::InvSqrt(LengthSquared);
			OutPlane = FPlane(-A * InvLength, -B * InvLength, -C * InvLength, D * InvLength);
			return true;
		}
		return false;
	}
} // namespace

bool FMatrix::GetFrustumNearPlane(FPlane& OutPlane) const
{
	return MakeFrustumPlane(M[0][2], M[1][2], M[2][2], M[3][2], OutPlane);
}

bool FMatrix::GetFrustumFarPlane(FPlane& OutPlane) const
{
	return MakeFrustumPlane(M[0][3] - M[0][2], M[1][3] - M[1][2], M[2][3] - M[2][2], M[3][3] - M[3][2], OutPlane);
}

bool FMatrix::GetFrustumLeftPlane(FPlane& OutPlane) const
{
	return MakeFrustumPlane(-M[0][3] - M[0][0], -M[1][3] - M[1][0], -M[2][3] - M[2][0], -M[3][3] - M[3][0], OutPlane);
}

bool FMatrix::GetFrustumRightPlane(FPlane& OutPlane) const
{
	return MakeFrustumPlane(M[0][0] - M[0][3], M[1][0] - M[1][3], M[2][0] - M[2][3], M[3][0] - M[3][3], OutPlane);
}

bool FMatrix::GetFrustumTopPlane(FPlane& OutPlane) const
{
	return MakeFrustumPlane(M[0][1] - M[0][3], M[1][1] - M[1][3], M[2][1] - M[2][3], M[3][1] - M[3][3], OutPlane);
}

bool FMatrix::GetFrustumBottomPlane(FPlane& OutPlane) const
{
	return MakeFrustumPlane(-M[0][3] - M[0][1], -M[1][3] - M[1][1], -M[2][3] - M[2][1], -M[3][3] - M[3][1], OutPlane);
}

void FMatrix::Mirror(EAxis::Type MirrorAxis, EAxis::Type FlipAxis)
{
	int32 Column = -1;
	if (MirrorAxis == EAxis::X)
	{
		Column = 0;
	}
	else if (MirrorAxis == EAxis::Y)
	{
		Column = 1;
	}
	else if (MirrorAxis == EAxis::Z)
	{
		Column = 2;
	}
	if (Column >= 0)
	{
		M[0][Column] *= -1.f;
		M[1][Column] *= -1.f;
		M[2][Column] *= -1.f;
		M[3][Column] *= -1.f;
	}

	int32 Row = -1;
	if (FlipAxis == EAxis::X)
	{
		Row = 0;
	}
	else if (FlipAxis == EAxis::Y)
	{
		Row = 1;
	}
	else if (FlipAxis == EAxis::Z)
	{
		Row = 2;
	}
	if (Row >= 0)
	{
		M[Row][0] *= -1.f;
		M[Row][1] *= -1.f;
		M[Row][2] *= -1.f;
	}
}

FString FMatrix::ToString() const
{
	FString Output;
	for (int32 Row = 0; Row < 4; ++Row)
	{
		Output += FString::Printf(
			"[%g %g %g %g] ", double(M[Row][0]), double(M[Row][1]), double(M[Row][2]), double(M[Row][3]));
	}
	return Output;
}

uint32 FMatrix::ComputeHash() const
{
	uint32 Data[16];
	std::memcpy(Data, M, sizeof(Data));

	uint32 Ret = 0;
	for (uint32 Index = 0; Index < 16; ++Index)
	{
		Ret ^= Data[Index] + Index;
	}
	return Ret;
}

FLookFromMatrix::FLookFromMatrix(const FVector& EyePosition, const FVector& LookDirection, const FVector& UpVector)
{
	const FVector ZAxis = LookDirection.GetSafeNormal();
	const FVector XAxis = (UpVector ^ ZAxis).GetSafeNormal();
	const FVector YAxis = ZAxis ^ XAxis;

	for (int32 RowIndex = 0; RowIndex < 3; RowIndex++)
	{
		M[RowIndex][0] = (&XAxis.X)[RowIndex];
		M[RowIndex][1] = (&YAxis.X)[RowIndex];
		M[RowIndex][2] = (&ZAxis.X)[RowIndex];
		M[RowIndex][3] = 0.0f;
	}
	M[3][0] = -EyePosition | XAxis;
	M[3][1] = -EyePosition | YAxis;
	M[3][2] = -EyePosition | ZAxis;
	M[3][3] = 1.0f;
}

// FRotationMatrix ----------------------------------------------------------------------------------------------------

namespace
{
	/** World Z unless Axis is nearly vertical, then world X (UE: the "try to use up if possible" pick). */
	FORCEINLINE FVector PickUpVector(const FVector& Axis)
	{
		return (FMath::Abs(Axis.Z) < (1.f - KINDA_SMALL_NUMBER)) ? FVector(0, 0, 1.f) : FVector(1.f, 0, 0);
	}

	/** Normalized Second, or a vector not parallel to First when they are (nearly) the same direction. */
	FORCEINLINE FVector PickSecondAxis(const FVector& First, const FVector& Second)
	{
		const FVector Norm = Second.GetSafeNormal();
		// If they're almost same, we need to find arbitrary vector.
		if (FMath::IsNearlyEqual(FMath::Abs(First | Norm), 1.f))
		{
			// Make sure we don't ever pick the same as First.
			return PickUpVector(First);
		}
		return Norm;
	}
} // namespace

FMatrix FRotationMatrix::MakeFromX(const FVector& XAxis)
{
	const FVector NewX = XAxis.GetSafeNormal();
	const FVector Up = PickUpVector(NewX);
	const FVector NewY = (Up ^ NewX).GetSafeNormal();
	const FVector NewZ = NewX ^ NewY;
	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromY(const FVector& YAxis)
{
	const FVector NewY = YAxis.GetSafeNormal();
	const FVector Up = PickUpVector(NewY);
	const FVector NewZ = (Up ^ NewY).GetSafeNormal();
	const FVector NewX = NewY ^ NewZ;
	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromZ(const FVector& ZAxis)
{
	const FVector NewZ = ZAxis.GetSafeNormal();
	const FVector Up = PickUpVector(NewZ);
	const FVector NewX = (Up ^ NewZ).GetSafeNormal();
	const FVector NewY = NewZ ^ NewX;
	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromXY(const FVector& XAxis, const FVector& YAxis)
{
	const FVector NewX = XAxis.GetSafeNormal();
	const FVector Norm = PickSecondAxis(NewX, YAxis);
	const FVector NewZ = (NewX ^ Norm).GetSafeNormal();
	const FVector NewY = NewZ ^ NewX;
	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromXZ(const FVector& XAxis, const FVector& ZAxis)
{
	const FVector NewX = XAxis.GetSafeNormal();
	const FVector Norm = PickSecondAxis(NewX, ZAxis);
	const FVector NewY = (Norm ^ NewX).GetSafeNormal();
	const FVector NewZ = NewX ^ NewY;
	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromYX(const FVector& YAxis, const FVector& XAxis)
{
	const FVector NewY = YAxis.GetSafeNormal();
	const FVector Norm = PickSecondAxis(NewY, XAxis);
	const FVector NewZ = (Norm ^ NewY).GetSafeNormal();
	const FVector NewX = NewY ^ NewZ;
	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromYZ(const FVector& YAxis, const FVector& ZAxis)
{
	const FVector NewY = YAxis.GetSafeNormal();
	const FVector Norm = PickSecondAxis(NewY, ZAxis);
	const FVector NewX = (NewY ^ Norm).GetSafeNormal();
	const FVector NewZ = NewX ^ NewY;
	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromZX(const FVector& ZAxis, const FVector& XAxis)
{
	const FVector NewZ = ZAxis.GetSafeNormal();
	const FVector Norm = PickSecondAxis(NewZ, XAxis);
	const FVector NewY = (NewZ ^ Norm).GetSafeNormal();
	const FVector NewX = NewY ^ NewZ;
	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromZY(const FVector& ZAxis, const FVector& YAxis)
{
	const FVector NewZ = ZAxis.GetSafeNormal();
	const FVector Norm = PickSecondAxis(NewZ, YAxis);
	const FVector NewX = (Norm ^ NewZ).GetSafeNormal();
	const FVector NewY = NewZ ^ NewX;
	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

// FPlane -------------------------------------------------------------------------------------------------------------

FPlane::FPlane(FVector A, FVector B, FVector C)
	: FVector(((B - A) ^ (C - A)).GetSafeNormal())
{
	W = A | static_cast<const FVector&>(*this);
}

bool FPlane::Normalize(float Tolerance)
{
	const float SquareSum = X * X + Y * Y + Z * Z;
	if (SquareSum > Tolerance)
	{
		const float Scale = FMath::InvSqrt(SquareSum);
		X *= Scale;
		Y *= Scale;
		Z *= Scale;
		W *= Scale;
		return true;
	}
	return false;
}

FPlane FPlane::TransformBy(const FMatrix& M) const
{
	const FMatrix TmpTA = M.TransposeAdjoint();
	const float DetM = M.Determinant();
	return TransformByUsingAdjointT(M, DetM, TmpTA);
}

FPlane FPlane::TransformByUsingAdjointT(const FMatrix& M, float DetM, const FMatrix& TA) const
{
	FVector NewNorm = FVector(TA.TransformVector(*this)).GetSafeNormal();

	if (DetM < 0.f)
	{
		NewNorm *= -1.0f;
	}

	return FPlane(FVector(M.TransformPosition(*this * W)), NewNorm);
}

FString FPlane::ToString() const
{
	return FString::Printf("X=%3.3f Y=%3.3f Z=%3.3f W=%3.3f", double(X), double(Y), double(Z), double(W));
}

// FBoxSphereBounds ---------------------------------------------------------------------------------------------------

FBoxSphereBounds::FBoxSphereBounds(const FBox& Box, const FSphere& Sphere)
{
	Box.GetCenterAndExtents(Origin, BoxExtent);
	SphereRadius = FMath::Min(BoxExtent.Size(), (Sphere.Center - Origin).Size() + Sphere.W);
}

FBoxSphereBounds::FBoxSphereBounds(const FVector* Points, uint32 NumPoints)
{
	FBox BoundingBox(ForceInit);

	// Find an axis aligned bounding box for the points.
	for (uint32 PointIndex = 0; PointIndex < NumPoints; PointIndex++)
	{
		BoundingBox += Points[PointIndex];
	}

	BoundingBox.GetCenterAndExtents(Origin, BoxExtent);

	// Using the center of the bounding box as the origin of the sphere, find the radius of the bounding sphere.
	SphereRadius = 0.0f;

	for (uint32 PointIndex = 0; PointIndex < NumPoints; PointIndex++)
	{
		SphereRadius = FMath::Max(SphereRadius, (Points[PointIndex] - Origin).Size());
	}
}

FBoxSphereBounds FBoxSphereBounds::operator+(const FBoxSphereBounds& Other) const
{
	FBox BoundingBox(ForceInit);

	BoundingBox += (Origin - BoxExtent);
	BoundingBox += (Origin + BoxExtent);
	BoundingBox += (Other.Origin - Other.BoxExtent);
	BoundingBox += (Other.Origin + Other.BoxExtent);

	// Build a bounding sphere from the bounding box's origin and the radii of A and B.
	FBoxSphereBounds Result(BoundingBox);

	Result.SphereRadius = FMath::Min(Result.SphereRadius,
		FMath::Max((Origin - Result.Origin).Size() + SphereRadius,
			(Other.Origin - Result.Origin).Size() + Other.SphereRadius));

	return Result;
}

FBoxSphereBounds FBoxSphereBounds::TransformBy(const FMatrix& M) const
{
	const FVector Row0(M.M[0][0], M.M[0][1], M.M[0][2]);
	const FVector Row1(M.M[1][0], M.M[1][1], M.M[1][2]);
	const FVector Row2(M.M[2][0], M.M[2][1], M.M[2][2]);

	FBoxSphereBounds Result;
	Result.Origin = FVector(M.TransformPosition(Origin));
	Result.BoxExtent = (Row0 * BoxExtent.X).GetAbs() + (Row1 * BoxExtent.Y).GetAbs() + (Row2 * BoxExtent.Z).GetAbs();

	// Largest column length (UE computes it on the rows as SIMD lanes, which are the columns).
	const FVector MaxRadius = Row0 * Row0 + Row1 * Row1 + Row2 * Row2;
	Result.SphereRadius = FMath::Sqrt(MaxRadius.GetMax()) * SphereRadius;

	// For non-uniform scaling, computing sphere radius from a box results in a smaller sphere.
	const float BoxExtentMagnitude = Result.BoxExtent.Size();
	Result.SphereRadius = FMath::Min(Result.SphereRadius, BoxExtentMagnitude);

	return Result;
}

FBoxSphereBounds FBoxSphereBounds::TransformBy(const FTransform& M) const
{
	return TransformBy(M.ToMatrixWithScale());
}

FString FBoxSphereBounds::ToString() const
{
	return FString::Printf("Origin=%s, BoxExtent=(%s), SphereRadius=(%f)", *Origin.ToString(), *BoxExtent.ToString(),
		double(SphereRadius));
}

// FMath --------------------------------------------------------------------------------------------------------------

float FMath::FInterpTo(float Current, float Target, float DeltaTime, float InterpSpeed)
{
	// If no interp speed, jump to target value.
	if (InterpSpeed <= 0.f)
	{
		return Target;
	}

	// Distance to reach.
	const float Dist = Target - Current;

	// If distance is too small, just set the desired location.
	if (FMath::Square(Dist) < SMALL_NUMBER)
	{
		return Target;
	}

	// Delta Move, Clamp so we do not over shoot.
	const float DeltaMove = Dist * FMath::Clamp(DeltaTime * InterpSpeed, 0.f, 1.f);

	return Current + DeltaMove;
}

float FMath::FInterpConstantTo(float Current, float Target, float DeltaTime, float InterpSpeed)
{
	const float Dist = Target - Current;

	// If distance is too small, just set the desired location.
	if (FMath::Square(Dist) < SMALL_NUMBER)
	{
		return Target;
	}

	const float Step = InterpSpeed * DeltaTime;
	return Current + FMath::Clamp(Dist, -Step, Step);
}

float FMath::InterpEaseIn(float A, float B, float Alpha, float Exp)
{
	const float ModifiedAlpha = Pow(Alpha, Exp);
	return Lerp(A, B, ModifiedAlpha);
}

float FMath::InterpEaseOut(float A, float B, float Alpha, float Exp)
{
	const float ModifiedAlpha = 1.f - Pow(1.f - Alpha, Exp);
	return Lerp(A, B, ModifiedAlpha);
}

float FMath::InterpEaseInOut(float A, float B, float Alpha, float Exp)
{
	return Lerp(A, B,
		(Alpha < 0.5f) ? InterpEaseIn(0.f, 1.f, Alpha * 2.f, Exp) * 0.5f
					   : InterpEaseOut(0.f, 1.f, Alpha * 2.f - 1.f, Exp) * 0.5f + 0.5f);
}

float FMath::ClampAngle(float AngleDegrees, float MinAngleDegrees, float MaxAngleDegrees)
{
	const float MaxDelta = FRotator::ClampAxis(MaxAngleDegrees - MinAngleDegrees) * 0.5f; // 0..180
	const float RangeCenter = FRotator::ClampAxis(MinAngleDegrees + MaxDelta); // 0..360
	const float DeltaFromCenter = FRotator::NormalizeAxis(AngleDegrees - RangeCenter); // -180..180

	// Maybe clamp to nearest edge.
	if (DeltaFromCenter > MaxDelta)
	{
		return FRotator::NormalizeAxis(RangeCenter + MaxDelta);
	}
	if (DeltaFromCenter < -MaxDelta)
	{
		return FRotator::NormalizeAxis(RangeCenter - MaxDelta);
	}

	// Already in range, just return it.
	return FRotator::NormalizeAxis(AngleDegrees);
}

FVector FMath::VInterpTo(const FVector& Current, const FVector& Target, float DeltaTime, float InterpSpeed)
{
	// If no interp speed, jump to target value.
	if (InterpSpeed <= 0.f)
	{
		return Target;
	}

	// Distance to reach.
	const FVector Dist = Target - Current;

	// If distance is too small, just set the desired location.
	if (Dist.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return Target;
	}

	// Delta Move, Clamp so we do not over shoot.
	const FVector DeltaMove = Dist * FMath::Clamp(DeltaTime * InterpSpeed, 0.f, 1.f);

	return Current + DeltaMove;
}

FVector FMath::VInterpConstantTo(const FVector& Current, const FVector& Target, float DeltaTime, float InterpSpeed)
{
	const FVector Delta = Target - Current;
	const float DeltaM = Delta.Size();
	const float MaxStep = InterpSpeed * DeltaTime;

	if (DeltaM > MaxStep)
	{
		if (MaxStep > 0.f)
		{
			const FVector DeltaN = Delta / DeltaM;
			return Current + DeltaN * MaxStep;
		}
		return Current;
	}

	return Target;
}

FRotator FMath::RInterpTo(const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed)
{
	// If DeltaTime is 0, do not perform any interpolation (Location was already calculated for that frame).
	if (DeltaTime == 0.f || Current == Target)
	{
		return Current;
	}

	// If no interp speed, jump to target value.
	if (InterpSpeed <= 0.f)
	{
		return Target;
	}

	const float DeltaInterpSpeed = InterpSpeed * DeltaTime;

	const FRotator Delta = (Target - Current).GetNormalized();

	// If steps are too small, just return Target and assume we have reached our destination.
	if (Delta.IsNearlyZero())
	{
		return Target;
	}

	// Delta Move, Clamp so we do not over shoot.
	const FRotator DeltaMove = Delta * FMath::Clamp(DeltaInterpSpeed, 0.f, 1.f);
	return (Current + DeltaMove).GetNormalized();
}

FRotator FMath::RInterpConstantTo(const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed)
{
	// If DeltaTime is 0, do not perform any interpolation (Location was already calculated for that frame).
	if (DeltaTime == 0.f || Current == Target)
	{
		return Current;
	}

	// If no interp speed, jump to target value.
	if (InterpSpeed <= 0.f)
	{
		return Target;
	}

	const float DeltaInterpSpeed = InterpSpeed * DeltaTime;

	const FRotator DeltaMove = (Target - Current).GetNormalized();
	FRotator Result = Current;
	Result.Pitch += FMath::Clamp(DeltaMove.Pitch, -DeltaInterpSpeed, DeltaInterpSpeed);
	Result.Yaw += FMath::Clamp(DeltaMove.Yaw, -DeltaInterpSpeed, DeltaInterpSpeed);
	Result.Roll += FMath::Clamp(DeltaMove.Roll, -DeltaInterpSpeed, DeltaInterpSpeed);
	return Result.GetNormalized();
}

FQuat FMath::QInterpTo(const FQuat& Current, const FQuat& Target, float DeltaTime, float InterpSpeed)
{
	// If no interp speed, jump to target value.
	if (InterpSpeed <= 0.f)
	{
		return Target;
	}

	// If the values are nearly equal, just return Target and assume we have reached our destination.
	if (Current.Equals(Target))
	{
		return Target;
	}

	return FQuat::Slerp(Current, Target, FMath::Clamp(InterpSpeed * DeltaTime, 0.f, 1.f));
}

FQuat FMath::QInterpConstantTo(const FQuat& Current, const FQuat& Target, float DeltaTime, float InterpSpeed)
{
	// If no interp speed, jump to target value.
	if (InterpSpeed <= 0.f)
	{
		return Target;
	}

	// If the values are nearly equal, just return Target and assume we have reached our destination.
	if (Current.Equals(Target))
	{
		return Target;
	}

	const float DeltaInterpSpeed = FMath::Clamp(DeltaTime * InterpSpeed, 0.f, 1.f);
	const float AngularDistance = FMath::Max(SMALL_NUMBER, Target.AngularDistance(Current));
	const float Alpha = FMath::Clamp(DeltaInterpSpeed / AngularDistance, 0.f, 1.f);

	return FQuat::Slerp(Current, Target, Alpha);
}

FVector FMath::VRand()
{
	FVector Result;
	float L;

	do
	{
		// Check random vectors in the unit sphere so result is statistically uniform.
		Result.X = FRand() * 2.f - 1.f;
		Result.Y = FRand() * 2.f - 1.f;
		Result.Z = FRand() * 2.f - 1.f;
		L = Result.SizeSquared();
	} while (L > 1.0f || L < KINDA_SMALL_NUMBER);

	return Result * (1.0f / Sqrt(L));
}

namespace
{
	/** VRandCone with the two random numbers supplied (shared by FMath and FRandomStream). */
	FVector RandConeFromUV(const FVector& Dir, float ConeHalfAngleRad, float RandU, float RandV)
	{
		if (ConeHalfAngleRad <= 0.f)
		{
			return Dir.GetSafeNormal();
		}

		// Get spherical coords that have an even distribution over the unit sphere.
		// Method described at http://mathworld.wolfram.com/SpherePointPicking.html
		const float Theta = 2.f * PI * RandU;
		float Phi = FMath::Acos((2.f * RandV) - 1.f);

		// Restrict phi to [0, ConeHalfAngleRad]. This gives an even distribution of points on the surface of the
		// cone centered at the origin, pointing upward (z), with the desired angle.
		Phi = FMath::Fmod(Phi, ConeHalfAngleRad);

		// Get axes we need to rotate around.
		const FMatrix DirMat = FRotationMatrix(Dir.Rotation());
		// Note the axis translation, since we want the variation to be around X.
		const FVector DirZ = DirMat.GetScaledAxis(EAxis::X);
		const FVector DirY = DirMat.GetScaledAxis(EAxis::Y);

		FVector Result = Dir.RotateAngleAxis(Phi * 180.f / PI, DirY);
		Result = Result.RotateAngleAxis(Theta * 180.f / PI, DirZ);

		// Ensure it's a unit vector (might not have been passed in that way).
		return Result.GetSafeNormal();
	}
} // namespace

FVector FMath::VRandCone(const FVector& Dir, float ConeHalfAngleRad)
{
	if (ConeHalfAngleRad <= 0.f)
	{
		return Dir.GetSafeNormal();
	}
	const float RandU = FRand();
	const float RandV = FRand();
	return RandConeFromUV(Dir, ConeHalfAngleRad, RandU, RandV);
}

FVector FRandomStream::VRandCone(const FVector& Dir, float ConeHalfAngleRad) const
{
	if (ConeHalfAngleRad <= 0.f)
	{
		return Dir.GetSafeNormal();
	}
	const float RandU = FRand();
	const float RandV = FRand();
	return RandConeFromUV(Dir, ConeHalfAngleRad, RandU, RandV);
}

FVector FMath::ClosestPointOnSegment(const FVector& Point, const FVector& StartPoint, const FVector& EndPoint)
{
	const FVector Segment = EndPoint - StartPoint;
	const FVector VectToPoint = Point - StartPoint;

	// See if closest point is before StartPoint.
	const float Dot1 = VectToPoint | Segment;
	if (Dot1 <= 0)
	{
		return StartPoint;
	}

	// See if closest point is beyond EndPoint.
	const float Dot2 = Segment | Segment;
	if (Dot2 <= Dot1)
	{
		return EndPoint;
	}

	// Closest Point is within segment.
	return StartPoint + Segment * (Dot1 / Dot2);
}

float FMath::PointDistToSegment(const FVector& Point, const FVector& StartPoint, const FVector& EndPoint)
{
	const FVector ClosestPoint = ClosestPointOnSegment(Point, StartPoint, EndPoint);
	return (Point - ClosestPoint).Size();
}

float FMath::PointDistToSegmentSquared(const FVector& Point, const FVector& StartPoint, const FVector& EndPoint)
{
	const FVector ClosestPoint = ClosestPointOnSegment(Point, StartPoint, EndPoint);
	return (Point - ClosestPoint).SizeSquared();
}

float FMath::PointDistToLine(const FVector& Point, const FVector& Direction, const FVector& Origin)
{
	const FVector SafeDir = Direction.GetSafeNormal();
	const FVector OutClosestPoint = Origin + (SafeDir * ((Point - Origin) | SafeDir));
	return (OutClosestPoint - Point).Size();
}

FVector FMath::LinePlaneIntersection(const FVector& Point1, const FVector& Point2, const FPlane& Plane)
{
	return Point1 + (Point2 - Point1) * ((Plane.W - (Point1 | Plane)) / ((Point2 - Point1) | Plane));
}

FVector FMath::LinePlaneIntersection(
	const FVector& Point1, const FVector& Point2, const FVector& PlaneOrigin, const FVector& PlaneNormal)
{
	return Point1 + (Point2 - Point1) * (((PlaneOrigin - Point1) | PlaneNormal) / ((Point2 - Point1) | PlaneNormal));
}

bool FMath::LineBoxIntersection(const FBox& Box, const FVector& Start, const FVector& End, const FVector& Direction)
{
	return LineBoxIntersection(Box, Start, End, Direction, Direction.Reciprocal());
}

bool FMath::LineBoxIntersection(const FBox& Box, const FVector& Start, const FVector& End, const FVector& Direction,
	const FVector& OneOverDirection)
{
	FVector Time;
	bool bStartIsOutside = false;

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (Start[Axis] < Box.Min[Axis])
		{
			bStartIsOutside = true;
			if (End[Axis] >= Box.Min[Axis])
			{
				Time[Axis] = (Box.Min[Axis] - Start[Axis]) * OneOverDirection[Axis];
			}
			else
			{
				return false;
			}
		}
		else if (Start[Axis] > Box.Max[Axis])
		{
			bStartIsOutside = true;
			if (End[Axis] <= Box.Max[Axis])
			{
				Time[Axis] = (Box.Max[Axis] - Start[Axis]) * OneOverDirection[Axis];
			}
			else
			{
				return false;
			}
		}
		else
		{
			Time[Axis] = 0.0f;
		}
	}

	if (!bStartIsOutside)
	{
		return true;
	}

	const float MaxTime = Max3(Time.X, Time.Y, Time.Z);
	if (MaxTime >= 0.0f && MaxTime <= 1.0f)
	{
		const FVector Hit = Start + Direction * MaxTime;
		const float BoxSideThreshold = 0.1f;
		if (Hit.X > Box.Min.X - BoxSideThreshold && Hit.X < Box.Max.X + BoxSideThreshold &&
			Hit.Y > Box.Min.Y - BoxSideThreshold && Hit.Y < Box.Max.Y + BoxSideThreshold &&
			Hit.Z > Box.Min.Z - BoxSideThreshold && Hit.Z < Box.Max.Z + BoxSideThreshold)
		{
			return true;
		}
	}

	return false;
}

bool FMath::SphereAABBIntersection(const FVector& SphereCenter, float RadiusSquared, const FBox& AABB)
{
	return AABB.ComputeSquaredDistanceToPoint(SphereCenter) <= RadiusSquared;
}

FVector FMath::GetReflectionVector(const FVector& Direction, const FVector& SurfaceNormal)
{
	const FVector SafeNormal = SurfaceNormal.GetSafeNormal();
	return Direction - 2.f * (Direction | SafeNormal) * SafeNormal;
}
