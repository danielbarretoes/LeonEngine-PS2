#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

// Core math with reference values worked out by hand from the UE conventions (X forward, Y right, Z up; positive yaw
// turns X toward Y, positive pitch turns X toward Z, positive roll turns Y toward -Z). The same values must hold on
// every platform, PS2 included, so there are no NaN / infinity checks (the EE FPU has neither).

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr float MathTolerance = 1.e-4f;

	bool TestVector(FAutomationTestBase& Test, const TCHAR* What, const FVector& Actual, const FVector& Expected,
		float Tolerance = MathTolerance)
	{
		if (!Actual.Equals(Expected, Tolerance))
		{
			Test.AddError(
				FString::Printf("%s: expected (%s), got (%s)", What, *Expected.ToString(), *Actual.ToString()));
			return false;
		}
		return true;
	}

	bool TestQuat(FAutomationTestBase& Test, const TCHAR* What, const FQuat& Actual, const FQuat& Expected,
		float Tolerance = MathTolerance)
	{
		if (!Actual.Equals(Expected, Tolerance))
		{
			Test.AddError(
				FString::Printf("%s: expected (%s), got (%s)", What, *Expected.ToString(), *Actual.ToString()));
			return false;
		}
		return true;
	}

	bool TestRotator(FAutomationTestBase& Test, const TCHAR* What, const FRotator& Actual, const FRotator& Expected,
		float Tolerance = 1.e-3f)
	{
		if (!Actual.Equals(Expected, Tolerance))
		{
			Test.AddError(
				FString::Printf("%s: expected (%s), got (%s)", What, *Expected.ToString(), *Actual.ToString()));
			return false;
		}
		return true;
	}

	bool TestMatrix(FAutomationTestBase& Test, const TCHAR* What, const FMatrix& Actual, const FMatrix& Expected,
		float Tolerance = MathTolerance)
	{
		if (!Actual.Equals(Expected, Tolerance))
		{
			Test.AddError(FString::Printf("%s: expected %s, got %s", What, *Expected.ToString(), *Actual.ToString()));
			return false;
		}
		return true;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMathUtilityTest, "System.Core.Math.Utility",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMathUtilityTest::RunTest(const FString& Parameters)
{
	TestEqual("Clamp", FMath::Clamp(5, 0, 3), 3);
	TestEqual("Square", FMath::Square(3.f), 9.f);
	TestEqual("Lerp", FMath::Lerp(10.f, 20.f, 0.25f), 12.5f);
	TestEqual("GridSnap", FMath::GridSnap(17.f, 5.f), 15.f);
	TestEqual("UnwindDegrees", FMath::UnwindDegrees(370.f), 10.f);
	TestEqual("UnwindDegrees negative", FMath::UnwindDegrees(-190.f), 170.f);
	TestEqual("FindDeltaAngleDegrees wraps", FMath::FindDeltaAngleDegrees(350.f, 10.f), 20.f);
	TestEqual("ClampAngle inside", FMath::ClampAngle(10.f, -45.f, 45.f), 10.f);
	TestEqual("ClampAngle to max", FMath::ClampAngle(90.f, -45.f, 45.f), 45.f);
	TestEqual("ClampAngle across the wrap", FMath::ClampAngle(300.f, -45.f, 45.f), -45.f);
	TestEqual("FInterpTo", FMath::FInterpTo(0.f, 10.f, 0.1f, 5.f), 5.f);
	TestEqual("FInterpTo zero speed jumps", FMath::FInterpTo(0.f, 10.f, 0.1f, 0.f), 10.f);
	TestEqual("FInterpConstantTo", FMath::FInterpConstantTo(0.f, 10.f, 0.5f, 4.f), 2.f);
	TestEqual("GetMappedRangeValueClamped", FMath::GetMappedRangeValueClamped(0.f, 10.f, 100.f, 200.f, 15.f), 200.f);
	TestEqual("SmoothStep middle", FMath::SmoothStep(0.f, 1.f, 0.5f), 0.5f);
	TestEqual("FastAsin", FMath::FastAsin(0.5f), PI / 6.f);
	TestEqual("InterpEaseInOut middle", FMath::InterpEaseInOut(0.f, 1.f, 0.5f, 2.f), 0.5f);

	float S = 0.f;
	float C = 0.f;
	FMath::SinCos(&S, &C, HALF_PI);
	TestEqual("SinCos sin", S, 1.f);
	TestEqual("SinCos cos", C, 0.f);

	TestVector(*this, "ClosestPointOnSegment",
		FMath::ClosestPointOnSegment(FVector(5, 3, 0), FVector(0, 0, 0), FVector(10, 0, 0)), FVector(5, 0, 0));
	TestVector(*this, "ClosestPointOnSegment clamps",
		FMath::ClosestPointOnSegment(FVector(-5, 3, 0), FVector(0, 0, 0), FVector(10, 0, 0)), FVector(0, 0, 0));
	TestEqual(
		"PointDistToSegment", FMath::PointDistToSegment(FVector(5, 3, 0), FVector(0, 0, 0), FVector(10, 0, 0)), 3.f);
	TestVector(*this, "LinePlaneIntersection",
		FMath::LinePlaneIntersection(FVector(0, 0, 10), FVector(0, 0, -10), FPlane(FVector(0, 0, 2), FVector(0, 0, 1))),
		FVector(0, 0, 2));
	TestVector(*this, "GetReflectionVector", FMath::GetReflectionVector(FVector(1, 0, -1), FVector(0, 0, 1)),
		FVector(1, 0, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMathVectorTest, "System.Core.Math.Vector",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMathVectorTest::RunTest(const FString& Parameters)
{
	const FVector A(1, 2, 3);
	const FVector B(4, 5, 6);
	TestEqual("Dot", A | B, 32.f);
	TestVector(*this, "Cross", A ^ B, FVector(-3, 6, -3));
	TestVector(*this, "X ^ Y = Z", FVector::ForwardVector ^ FVector::RightVector, FVector::UpVector);
	TestEqual("Size", FVector(3, 4, 0).Size(), 5.f);
	TestEqual("Size2D", FVector(3, 4, 12).Size2D(), 5.f);
	TestVector(*this, "GetSafeNormal", FVector(0, 0, 5).GetSafeNormal(), FVector(0, 0, 1));
	TestVector(*this, "GetSafeNormal of zero", FVector::ZeroVector.GetSafeNormal(), FVector::ZeroVector);
	TestVector(*this, "GetClampedToMaxSize", FVector(0, 30, 40).GetClampedToMaxSize(10.f), FVector(0, 6, 8));
	TestVector(
		*this, "RotateAngleAxis X about Z", FVector(1, 0, 0).RotateAngleAxis(90.f, FVector(0, 0, 1)), FVector(0, 1, 0));
	TestVector(*this, "ProjectOnTo", FVector(3, 4, 0).ProjectOnTo(FVector(1, 0, 0)), FVector(3, 0, 0));
	TestVector(*this, "MirrorByVector", FVector(1, 1, 0).MirrorByVector(FVector(0, 1, 0)), FVector(1, -1, 0));

	const FRotator FromDirection = FVector(1, 1, 0).Rotation();
	TestRotator(*this, "Rotation of (1, 1, 0)", FromDirection, FRotator(0.f, 45.f, 0.f));
	TestRotator(*this, "Rotation of up", FVector(0, 0, 1).Rotation(), FRotator(90.f, 0.f, 0.f));
	TestQuat(*this, "ToOrientationQuat matches Rotation", FVector(1, 2, 3).ToOrientationQuat(),
		FVector(1, 2, 3).Rotation().Quaternion());

	FVector Axis1, Axis2;
	FVector(0, 0, 1).FindBestAxisVectors(Axis1, Axis2);
	TestEqual("FindBestAxisVectors orthogonal 1", Axis1 | FVector(0, 0, 1), 0.f);
	TestEqual("FindBestAxisVectors orthogonal 2", Axis2 | FVector(0, 0, 1), 0.f);

	TestEqual("ToString", FVector(1, 2.5f, -3).ToString(), TEXT("X=1.000 Y=2.500 Z=-3.000"));
	FVector Parsed;
	TestTrue("InitFromString", Parsed.InitFromString("X=1.5 Y=-2 Z=3"));
	TestVector(*this, "InitFromString value", Parsed, FVector(1.5f, -2.f, 3.f));
	TestFalse("InitFromString missing Z", Parsed.InitFromString("X=1 Y=2"));

	TestEqual("FVector2D GetRotated", FVector2D(1, 0).GetRotated(90.f).Y, 1.f);
	TestEqual("FIntVector from FVector truncates", FIntVector(FVector(1.9f, -1.9f, 2.1f)).Y, -1);
	TestEqual("FVector4 Dot3", FVector4::Dot3(FVector4(1, 2, 3, 100), FVector4(1, 1, 1, 100)), 6.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMathRotationTest, "System.Core.Math.Rotation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMathRotationTest::RunTest(const FString& Parameters)
{
	const float H = UE_INV_SQRT_2;

	// Reference quaternions of the three axis rotations (UE's FRotator::Quaternion).
	TestQuat(*this, "Yaw 90", FRotator(0.f, 90.f, 0.f).Quaternion(), FQuat(0.f, 0.f, H, H));
	TestQuat(*this, "Pitch 90", FRotator(90.f, 0.f, 0.f).Quaternion(), FQuat(0.f, -H, 0.f, H));
	TestQuat(*this, "Roll 90", FRotator(0.f, 0.f, 90.f).Quaternion(), FQuat(-H, 0.f, 0.f, H));

	// Directions.
	TestVector(*this, "Yaw 90 forward", FRotator(0.f, 90.f, 0.f).Vector(), FVector(0, 1, 0));
	TestVector(*this, "Pitch 90 forward", FRotator(90.f, 0.f, 0.f).Vector(), FVector(0, 0, 1));
	TestVector(*this, "Roll 90 turns right to down", FRotator(0.f, 0.f, 90.f).RotateVector(FVector(0, 1, 0)),
		FVector(0, 0, -1));
	TestVector(*this, "Quat yaw 90 rotates X", FRotator(0.f, 90.f, 0.f).Quaternion().RotateVector(FVector(1, 0, 0)),
		FVector(0, 1, 0));

	// Rotator -> quaternion -> rotator, away from the pitch singularity.
	const FRotator Rotators[] = {
		FRotator(30.f, 45.f, 60.f), FRotator(-80.f, 170.f, -120.f), FRotator(10.f, -100.f, 5.f)};
	for (const FRotator& R : Rotators)
	{
		TestRotator(*this, "Rotator round trip", R.Quaternion().Rotator(), R);
		TestRotator(*this, "Matrix Rotator", FRotationMatrix(R).Rotator(), R);
		TestQuat(*this, "Matrix to quat", FQuat(FRotationMatrix(R)), R.Quaternion());
		TestMatrix(*this, "Rotation matrix from rotator and from quat", FRotationMatrix(R),
			FQuatRotationMatrix(R.Quaternion()));
		TestVector(*this, "RotateVector matches quat", R.RotateVector(FVector(1, 2, 3)),
			R.Quaternion().RotateVector(FVector(1, 2, 3)));
		TestVector(*this, "UnrotateVector undoes RotateVector", R.UnrotateVector(R.RotateVector(FVector(1, 2, 3))),
			FVector(1, 2, 3));
		TestMatrix(*this, "FInverseRotationMatrix", FInverseRotationMatrix(R), FRotationMatrix(R).GetTransposed());
	}

	// Composition: A * B applies B first.
	const FQuat Yaw90(FVector(0, 0, 1), HALF_PI);
	const FQuat Roll90(FVector(1, 0, 0), HALF_PI);
	TestVector(*this, "Quat composition order", (Yaw90 * Roll90).RotateVector(FVector(0, 1, 0)),
		Yaw90.RotateVector(Roll90.RotateVector(FVector(0, 1, 0))));
	TestQuat(*this, "Inverse", Yaw90 * Yaw90.Inverse(), FQuat::Identity);

	// Slerp and FindBetween.
	TestQuat(*this, "Slerp half way", FQuat::Slerp(FQuat::Identity, FQuat(FVector(0, 0, 1), HALF_PI), 0.5f),
		FQuat(FVector(0, 0, 1), HALF_PI * 0.5f));
	TestVector(*this, "FindBetween",
		FQuat::FindBetween(FVector(1, 0, 0), FVector(0, 0, 5)).RotateVector(FVector(1, 0, 0)), FVector(0, 0, 1));
	TestVector(*this, "FindBetween opposite",
		FQuat::FindBetweenNormals(FVector(1, 0, 0), FVector(-1, 0, 0)).RotateVector(FVector(1, 0, 0)),
		FVector(-1, 0, 0));
	TestEqual("AngularDistance", FQuat::Identity.AngularDistance(Yaw90), HALF_PI);

	FVector Axis;
	float Angle = 0.f;
	Yaw90.ToAxisAndAngle(Axis, Angle);
	TestVector(*this, "ToAxisAndAngle axis", Axis, FVector(0, 0, 1));
	TestEqual("ToAxisAndAngle angle", Angle, HALF_PI);

	// At pitch 90 yaw and roll describe the same turn; the rotator differs but the orientation must not.
	const FQuat Singular = FRotator(90.f, 30.f, 0.f).Quaternion();
	TestQuat(*this, "Singularity pitch 90", Singular.Rotator().Quaternion(), Singular);
	TestRotator(*this, "RInterpTo", FMath::RInterpTo(FRotator(0.f, 0.f, 0.f), FRotator(0.f, 90.f, 0.f), 0.1f, 5.f),
		FRotator(0.f, 45.f, 0.f));
	TestRotator(*this, "Normalize", FRotator(0.f, 270.f, -190.f).GetNormalized(), FRotator(0.f, -90.f, 170.f));

	FRotator ParsedRot;
	TestTrue("Rotator InitFromString", ParsedRot.InitFromString(FRotator(10.f, 20.f, 30.f).ToString()));
	TestRotator(*this, "Rotator InitFromString value", ParsedRot, FRotator(10.f, 20.f, 30.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMathMatrixTest, "System.Core.Math.Matrix",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMathMatrixTest::RunTest(const FString& Parameters)
{
	// Row vectors: translation lives in the last row, and A * B applies A first.
	const FMatrix T = FTranslationMatrix(FVector(10, 20, 30));
	TestEqual("Translation row", T.M[3][1], 20.f);
	TestVector(*this, "TransformPosition", FVector(T.TransformPosition(FVector(1, 2, 3))), FVector(11, 22, 33));
	TestVector(
		*this, "TransformVector ignores translation", FVector(T.TransformVector(FVector(1, 2, 3))), FVector(1, 2, 3));

	const FMatrix S = FScaleMatrix(FVector(2, 3, 4));
	TestEqual("Determinant of scale", S.Determinant(), 24.f);
	TestVector(
		*this, "Scale then translate", FVector((S * T).TransformPosition(FVector(1, 1, 1))), FVector(12, 23, 34));
	TestVector(
		*this, "Translate then scale", FVector((T * S).TransformPosition(FVector(1, 1, 1))), FVector(22, 63, 124));

	const FMatrix M =
		FScaleRotationTranslationMatrix(FVector(2, 3, 4), FRotator(30.f, 45.f, 60.f), FVector(10, -20, 30));
	TestMatrix(*this, "M * Inverse", M * M.Inverse(), FMatrix::Identity);
	TestMatrix(*this, "Inverse * M", M.Inverse() * M, FMatrix::Identity);
	TestVector(*this, "InverseTransformPosition",
		M.InverseTransformPosition(FVector(M.TransformPosition(FVector(1, 2, 3)))), FVector(1, 2, 3), 1.e-3f);
	TestVector(*this, "GetScaleVector", M.GetScaleVector(), FVector(2, 3, 4));
	TestVector(*this, "GetOrigin", M.GetOrigin(), FVector(10, -20, 30));
	TestRotator(*this, "Rotator ignores uniform scale",
		FScaleRotationTranslationMatrix(FVector(3.f), FRotator(30.f, 45.f, 60.f), FVector(0.f)).Rotator(),
		FRotator(30.f, 45.f, 60.f));
	TestMatrix(*this, "Singular Inverse is Identity", FScaleMatrix(0.f).Inverse(), FMatrix::Identity);
	TestEqual("RotDeterminant of a rotation", FRotationMatrix(FRotator(30.f, 45.f, 60.f)).RotDeterminant(), 1.f);

	FMatrix Unscaled = M;
	TestVector(*this, "ExtractScaling", Unscaled.ExtractScaling(), FVector(2, 3, 4));
	TestMatrix(*this, "ExtractScaling leaves the rotation", Unscaled,
		FRotationTranslationMatrix(FRotator(30.f, 45.f, 60.f), FVector(10, -20, 30)));

	// Rotation about a point keeps the point fixed.
	const FVector Pivot(5, 5, 0);
	TestVector(*this, "FRotationAboutPointMatrix",
		FVector(FRotationAboutPointMatrix(FRotator(0.f, 90.f, 0.f), Pivot).TransformPosition(Pivot)), Pivot);

	// MakeFromX keeps world Z as up.
	const FMatrix FromX = FRotationMatrix::MakeFromX(FVector(0, 1, 0));
	TestVector(*this, "MakeFromX X", FromX.GetScaledAxis(EAxis::X), FVector(0, 1, 0));
	TestVector(*this, "MakeFromX Z is up", FromX.GetScaledAxis(EAxis::Z), FVector(0, 0, 1));
	TestVector(*this, "MakeFromXZ Y",
		FRotationMatrix::MakeFromXZ(FVector(1, 0, 0), FVector(0, 0, 1)).GetScaledAxis(EAxis::Y), FVector(0, 1, 0));

	// A view matrix puts the eye at the origin, looking down +Z.
	const FMatrix View = FLookAtMatrix(FVector(0, 0, 0), FVector(10, 0, 0), FVector(0, 0, 1));
	TestVector(*this, "LookAt target on +Z", FVector(View.TransformPosition(FVector(10, 0, 0))), FVector(0, 0, 10));

	// UE clip space: depth 0 at the near plane, 1 at the far plane.
	const FMatrix Projection = FPerspectiveMatrix(HALF_PI * 0.5f, 1.f, 1.f, 10.f, 1000.f);
	const FVector4 Near = Projection.TransformFVector4(FVector4(0, 0, 10, 1));
	const FVector4 Far = Projection.TransformFVector4(FVector4(0, 0, 1000, 1));
	TestEqual("Perspective near depth", Near.Z / Near.W, 0.f);
	TestEqual("Perspective far depth", Far.Z / Far.W, 1.f);

	FPlane NearPlane;
	TestTrue("GetFrustumNearPlane", Projection.GetFrustumNearPlane(NearPlane));
	TestEqual("Near plane faces the camera", NearPlane.Z, -1.f);

	// Planes.
	const FPlane Ground(FVector(0, 0, 0), FVector(1, 0, 0), FVector(0, 1, 0));
	TestVector(*this, "Plane from points", Ground, FVector(0, 0, 1));
	TestEqual("PlaneDot", Ground.PlaneDot(FVector(3, 4, 5)), 5.f);
	const FPlane Moved = Ground.TransformBy(FTranslationMatrix(FVector(0, 0, 7)));
	TestEqual("Plane TransformBy W", Moved.W, 7.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMathTransformTest, "System.Core.Math.Transform",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMathTransformTest::RunTest(const FString& Parameters)
{
	const FTransform A(FRotator(0.f, 90.f, 0.f), FVector(100, 0, 0), FVector(2, 2, 2));
	const FTransform B(FRotator(30.f, 0.f, 10.f), FVector(0, 50, -10), FVector(1, 1, 1));
	const FVector P(1, 2, 3);

	// Scale, then rotate, then translate.
	TestVector(*this, "TransformPosition", A.TransformPosition(FVector(1, 0, 0)), FVector(100, 2, 0));
	TestVector(*this, "TransformVector", A.TransformVector(FVector(1, 0, 0)), FVector(0, 2, 0));
	TestVector(*this, "Matches ToMatrixWithScale", A.TransformPosition(P),
		FVector(A.ToMatrixWithScale().TransformPosition(P)));

	// A * B applies A first, like the matrices.
	TestVector(*this, "Multiply", (A * B).TransformPosition(P), B.TransformPosition(A.TransformPosition(P)));
	TestMatrix(*this, "Multiply matches matrices", (A * B).ToMatrixWithScale(),
		A.ToMatrixWithScale() * B.ToMatrixWithScale(), 1.e-3f);

	TestVector(*this, "Inverse", A.Inverse().TransformPosition(A.TransformPosition(P)), P);
	TestVector(*this, "InverseTransformPosition", A.InverseTransformPosition(A.TransformPosition(P)), P);
	TestTrue("GetRelativeTransform", (A * B).GetRelativeTransform(B).Equals(A, 1.e-3f));
	TestTrue("GetRelativeTransformReverse", A.GetRelativeTransformReverse(A * B).Equals(B, 1.e-3f));
	TestTrue("From matrix", FTransform(A.ToMatrixWithScale()).Equals(A, 1.e-3f));

	// A negative scale goes through the matrix path.
	const FTransform Mirrored(FQuat::Identity, FVector(0, 0, 0), FVector(-1, 1, 1));
	TestVector(*this, "Negative scale multiply", (Mirrored * B).TransformPosition(P),
		B.TransformPosition(Mirrored.TransformPosition(P)));

	FTransform Blended;
	Blended.Blend(FTransform(FVector(0, 0, 0)), FTransform(FVector(10, 0, 0)), 0.5f);
	TestVector(*this, "Blend", Blended.GetTranslation(), FVector(5, 0, 0));

	FTransform Parsed;
	TestTrue("InitFromString", Parsed.InitFromString(A.ToString()));
	TestTrue("InitFromString value", Parsed.Equals(A, 1.e-3f));
	TestFalse("InitFromString rejects garbage", Parsed.InitFromString("1,2|3"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMathBoundsTest, "System.Core.Math.Bounds",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMathBoundsTest::RunTest(const FString& Parameters)
{
	FBox Box(ForceInit);
	TestFalse("Empty box is invalid", Box.IsValid != 0);
	Box += FVector(1, 2, 3);
	Box += FVector(-1, 0, 5);
	TestVector(*this, "Box Min", Box.Min, FVector(-1, 0, 3));
	TestVector(*this, "Box Max", Box.Max, FVector(1, 2, 5));
	TestVector(*this, "Box center", Box.GetCenter(), FVector(0, 1, 4));
	TestTrue("IsInside", Box.IsInside(FVector(0, 1, 4)));
	TestFalse("IsInside on the face", Box.IsInside(FVector(1, 1, 4)));
	TestTrue("IsInsideOrOn on the face", Box.IsInsideOrOn(FVector(1, 1, 4)));

	const FBox Unit(FVector(-1), FVector(1));
	const FBox Rotated = Unit.TransformBy(FRotationMatrix(FRotator(0.f, 45.f, 0.f)));
	TestEqual("Rotated box grows", Rotated.Max.X, UE_SQRT_2);
	TestEqual("Rotated box keeps Z", Rotated.Max.Z, 1.f);
	TestTrue("Intersect", Unit.Intersect(FBox(FVector(0.5f), FVector(3))));
	TestVector(*this, "Overlap", Unit.Overlap(FBox(FVector(0.5f), FVector(3))).Min, FVector(0.5f));
	TestFalse("No intersect", Unit.Intersect(FBox(FVector(2), FVector(3))));
	TestEqual("ComputeSquaredDistanceToPoint", Unit.ComputeSquaredDistanceToPoint(FVector(3, 0, 0)), 4.f);
	TestTrue("LineBoxIntersection",
		FMath::LineBoxIntersection(Unit, FVector(-5, 0, 0), FVector(5, 0, 0), FVector(10, 0, 0)));
	TestFalse("LineBoxIntersection short",
		FMath::LineBoxIntersection(Unit, FVector(-5, 0, 0), FVector(-3, 0, 0), FVector(2, 0, 0)));

	const FTransform Move(FQuat::Identity, FVector(10, 0, 0), FVector(2));
	TestVector(*this, "InverseTransformBy", Unit.TransformBy(Move).InverseTransformBy(Move).Max, FVector(1), 1.e-3f);

	FSphere Sphere(FVector(0, 0, 0), 1.f);
	Sphere += FSphere(FVector(4, 0, 0), 1.f);
	TestVector(*this, "Sphere union center", Sphere.Center, FVector(2, 0, 0));
	TestEqual("Sphere union radius", Sphere.W, 3.f);
	TestTrue("Sphere IsInside", Sphere.IsInside(FVector(4.5f, 0, 0)));
	TestEqual("Sphere TransformBy scales", FSphere(FVector(0), 1.f).TransformBy(FScaleMatrix(3.f)).W, 3.f);

	const FBoxSphereBounds Bounds(Unit);
	TestEqual("Bounds radius", Bounds.SphereRadius, FMath::Sqrt(3.f));
	const FBoxSphereBounds Moved = Bounds.TransformBy(FTranslationMatrix(FVector(0, 0, 10)));
	TestVector(*this, "Bounds TransformBy origin", Moved.Origin, FVector(0, 0, 10));
	const FBoxSphereBounds Union = Bounds + Moved;
	TestVector(*this, "Bounds union extent", Union.BoxExtent, FVector(1, 1, 6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMathColorTest, "System.Core.Math.Color",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMathColorTest::RunTest(const FString& Parameters)
{
	// Every sRGB byte survives the trip through linear.
	int32 Mismatches = 0;
	for (int32 Value = 0; Value < 256; ++Value)
	{
		const FColor Original(uint8(Value), uint8(255 - Value), uint8(Value / 2), uint8(Value));
		if (FLinearColor(Original).ToFColor(true) != Original)
		{
			++Mismatches;
		}
	}
	TestEqual("sRGB round trip mismatches", Mismatches, 0);

	TestEqual("sRGB 128 to linear", FLinearColor(FColor(128, 128, 128)).R, 0.2158605f);
	TestEqual("FromHex", FColor::FromHex("#FF8000").DWColor(), FColor(255, 128, 0, 255).DWColor());
	TestEqual("FromHex short", int32(FColor::FromHex("F80").G), 0x88);
	TestEqual("FromHex with alpha", int32(FColor::FromHex("10203040").A), 0x40);
	TestEqual("ToHex", FColor(1, 2, 3, 4).ToHex(), TEXT("01020304"));
	TestEqual("DWColor is ARGB", FColor(0x11, 0x22, 0x33, 0x44).DWColor(), 0x44112233u);

	const FLinearColor HSV = FLinearColor(1.f, 0.5f, 0.f).LinearRGBToHSV();
	TestEqual("Hue", HSV.R, 30.f);
	TestEqual("Saturation", HSV.G, 1.f);
	TestEqual("Value", HSV.B, 1.f);
	TestTrue("HSV round trip", HSV.HSVToLinearRGB().Equals(FLinearColor(1.f, 0.5f, 0.f)));
	TestEqual("MakeRedToGreen middle", int32(FColor::MakeRedToGreenColorFromScalar(0.5f).R), 255);

	FLinearColor Parsed;
	TestTrue("InitFromString", Parsed.InitFromString("(R=0.5,G=0.25,B=1)"));
	TestEqual("InitFromString default alpha", Parsed.A, 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMathRandomStreamTest, "System.Core.Math.RandomStream",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMathRandomStreamTest::RunTest(const FString& Parameters)
{
	// The sequence for seed 42 is fixed: gameplay replays depend on it being the same on every platform.
	FRandomStream Stream(42);
	TestEqual("First fraction", Stream.GetFraction(), 0.131058931f, 1.e-7f);
	TestEqual("Second unsigned", Stream.GetUnsignedInt(), 0x35f4816cu);
	TestEqual("Current seed", uint32(Stream.GetCurrentSeed()), 0x35f4816cu);

	Stream.Reset();
	TestEqual("Reset replays", Stream.GetFraction(), 0.131058931f, 1.e-7f);

	bool bInRange = true;
	for (int32 Index = 0; Index < 1000; ++Index)
	{
		const int32 Value = Stream.RandRange(-3, 3);
		bInRange &= Value >= -3 && Value <= 3;
		const float Fraction = Stream.FRand();
		bInRange &= Fraction >= 0.f && Fraction < 1.f;
	}
	TestTrue("RandRange and FRand stay in range", bInRange);
	TestEqual("Unit vector", Stream.GetUnitVector().Size(), 1.f);
	TestTrue("VRandCone stays in the cone",
		(Stream.VRandCone(FVector(1, 0, 0), FMath::DegreesToRadians(10.f)) | FVector(1, 0, 0)) >=
			FMath::Cos(FMath::DegreesToRadians(10.f)) - 1.e-4f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
