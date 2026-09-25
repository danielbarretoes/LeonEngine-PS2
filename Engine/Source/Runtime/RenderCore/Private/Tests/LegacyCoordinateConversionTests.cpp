#include "CoreMinimal.h"
#include "LegacyCoordinateConversion.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * The legacy rotation about one axis (0 = X, 1 = Y, 2 = Z) from sin and cos, as glm::rotate built it. FMatrix row
	 * I is the image of axis I.
	 */
	FMatrix LegacyAxisRotation(int32 Axis, float Degrees)
	{
		const float Radians = FMath::DegreesToRadians(Degrees);
		const float C = FMath::Cos(Radians);
		const float S = FMath::Sin(Radians);
		FMatrix M = FMatrix::Identity;
		const int32 U = (Axis + 1) % 3;
		const int32 V = (Axis + 2) % 3;
		M.M[U][U] = C;
		M.M[U][V] = S;
		M.M[V][U] = -S;
		M.M[V][V] = C;
		return M;
	}

	/** The legacy model matrix, glm T * Rx * Ry * Rz * S, written as FMatrix products (applied left to right). */
	FMatrix LegacyModelMatrix(const FVector& Position, const FVector& EulerDegrees, const FVector& Scale)
	{
		return FScaleMatrix(Scale) * LegacyAxisRotation(2, EulerDegrees.Z) * LegacyAxisRotation(1, EulerDegrees.Y) *
			LegacyAxisRotation(0, EulerDegrees.X) * FTranslationMatrix(Position);
	}

	/** The legacy light direction from pitch and yaw degrees. */
	FVector LegacyLightDirection(float PitchDegrees, float YawDegrees)
	{
		const float Pitch = FMath::DegreesToRadians(PitchDegrees);
		const float Yaw = FMath::DegreesToRadians(YawDegrees);
		const float CosPitch = FMath::Cos(Pitch);
		const FVector Direction(FMath::Sin(Yaw) * CosPitch, -FMath::Sin(Pitch), FMath::Cos(Yaw) * CosPitch);
		return Direction / Direction.Size();
	}

	bool MatricesMatch(
		FAutomationTestBase& Test, const FString& What, const FMatrix& Actual, const FMatrix& Expected, float Tolerance)
	{
		for (int32 Index = 0; Index < 16; ++Index)
		{
			const float Value = Actual.M[Index / 4][Index % 4];
			const float Reference = Expected.M[Index / 4][Index % 4];
			if (FMath::Abs(Value - Reference) > Tolerance)
			{
				Test.AddError(FString::Printf("%s: element %d is %.9g, expected %.9g", *What, Index,
					static_cast<double>(Value), static_cast<double>(Reference)));
				return false;
			}
		}
		return true;
	}

	/** Euler triples in the range ToLegacyEulerXYZ returns them in (X and Z small, so none is ambiguous). */
	const FVector CanonicalEulers[] = {FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 90.0f, 0.0f),
		FVector(0.0f, 150.0f, 0.0f), FVector(0.0f, -170.0f, 0.0f), FVector(0.0f, 180.0f, 0.0f),
		FVector(30.0f, -45.0f, 60.0f), FVector(-20.0f, 10.0f, -75.0f), FVector(89.0f, 45.0f, 0.0f),
		FVector(12.5f, 0.0f, 0.0f), FVector(0.0f, 0.0f, -33.0f)};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyCoordinateConversionIdentityTest,
	"System.RenderCore.LegacyCoordinateConversion.Identity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyCoordinateConversionIdentityTest::RunTest(const FString& Parameters)
{
	// A default legacy transform is the identity, and each conversion undoes its inverse.
	const FMatrix M =
		FLegacyCoordinateConversion::ConvertTransform(FVector::ZeroVector, FVector::ZeroVector, FVector::OneVector)
			.ToMatrixWithScale();
	MatricesMatch(*this, "Identity", M, FMatrix::Identity, 0.0f);

	const FVector Legacy(1.5f, -2.0f, 3.25f);
	TestTrue("Position",
		FLegacyCoordinateConversion::ToLegacyPosition(FLegacyCoordinateConversion::ConvertPosition(Legacy))
			.Equals(Legacy, 1.0e-6f));
	TestTrue("Direction",
		FLegacyCoordinateConversion::ToLegacyDirection(FLegacyCoordinateConversion::ConvertDirection(Legacy))
			.Equals(Legacy, 1.0e-6f));
	TestTrue("Scale",
		FLegacyCoordinateConversion::ToLegacyScale(FLegacyCoordinateConversion::ConvertScale(Legacy))
			.Equals(Legacy, 1.0e-6f));
	TestEqual("Length", FLegacyCoordinateConversion::ToLegacyLength(FLegacyCoordinateConversion::ConvertLength(2.5f)),
		2.5f, 1.0e-6f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyCoordinateConversionTranslationYawTest,
	"System.RenderCore.LegacyCoordinateConversion.TranslationYaw",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyCoordinateConversionTranslationYawTest::RunTest(const FString& Parameters)
{
	// The position lands in the translation row; 90 degrees about legacy Y sends local +X to -Z.
	const FTransform Transform = FLegacyCoordinateConversion::ConvertTransform(
		FVector(2.0f, 3.0f, 4.0f), FVector(0.0f, 90.0f, 0.0f), FVector::OneVector);
	const FMatrix M = Transform.ToMatrixWithScale();
	TestEqual("Translation x", M.M[3][0], 2.0f);
	TestEqual("Translation y", M.M[3][1], 3.0f);
	TestEqual("Translation z", M.M[3][2], 4.0f);
	TestEqual("X axis x", M.M[0][0], 0.0f, 1.0e-6f);
	TestEqual("X axis z", M.M[0][2], -1.0f, 1.0e-6f);
	TestTrue("Local +X goes to -Z",
		Transform.TransformVector(FVector(1.0f, 0.0f, 0.0f)).Equals(FVector(0.0f, 0.0f, -1.0f), 1.0e-6f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyCoordinateConversionZeroScaleTest,
	"System.RenderCore.LegacyCoordinateConversion.ZeroScale",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyCoordinateConversionZeroScaleTest::RunTest(const FString& Parameters)
{
	// A scale closer to zero than 1e-4 is kept at +-1e-4, so the normal matrix (the model's inverse) stays finite.
	const FTransform Transform = FLegacyCoordinateConversion::ConvertTransform(
		FVector::ZeroVector, FVector::ZeroVector, FVector(0.0f, 1.0f, -1.0e-6f));
	TestEqual("Scale x sanitised", Transform.GetScale3D().X, 1.0e-4f);
	TestEqual("Scale y kept", Transform.GetScale3D().Y, 1.0f);
	TestEqual("Scale z keeps its sign", Transform.GetScale3D().Z, -1.0e-4f);
	const FMatrix Inverse = Transform.ToMatrixWithScale().Inverse();
	TestTrue("Normal matrix finite", FMath::IsFinite(Inverse.M[0][0]) && FMath::IsFinite(Inverse.M[2][2]));
	TestEqual("Normal matrix not the singular fallback", Inverse.M[0][0], 1.0e4f, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyCoordinateConversionModelMatrixTest,
	"System.RenderCore.LegacyCoordinateConversion.ModelMatrix",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyCoordinateConversionModelMatrixTest::RunTest(const FString& Parameters)
{
	// ConvertTransform builds the legacy model matrix T * Rx * Ry * Rz * S: glm's floats, and the sin / cos formula.
	const float GlmTrs[16] = {0.707106709f, 1.1464467f, 1.47839785f, 0, -0.306186229f, 0.369599462f, -0.140165031f, 0,
		-1.06066012f, -0.530330062f, 0.918558598f, 0, 1, -2, 3.5f, 1};
	FMatrix Glm;
	FMemory::Memcpy(&Glm.M[0][0], GlmTrs, sizeof(GlmTrs));
	MatricesMatch(*this, "glm translate / rotate / scale",
		FLegacyCoordinateConversion::ConvertTransform(
			FVector(1.0f, -2.0f, 3.5f), FVector(30.0f, -45.0f, 60.0f), FVector(2.0f, 0.5f, 1.5f))
			.ToMatrixWithScale(),
		Glm, 1.0e-6f);

	const FVector Position(-4.0f, 0.25f, 7.5f);
	const FVector Scale(0.5f, 3.0f, 1.25f);
	constexpr float Tolerance = 3.0e-6f; // 1e-6 per unit of the largest scale
	for (const FVector& Euler : CanonicalEulers)
	{
		MatricesMatch(*this, FString::Printf("Euler (%g, %g, %g)", Euler.X, Euler.Y, Euler.Z),
			FLegacyCoordinateConversion::ConvertTransform(Position, Euler, Scale).ToMatrixWithScale(),
			LegacyModelMatrix(Position, Euler, Scale), Tolerance);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyCoordinateConversionEulerRoundTripTest,
	"System.RenderCore.LegacyCoordinateConversion.EulerRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyCoordinateConversionEulerRoundTripTest::RunTest(const FString& Parameters)
{
	// Euler angles come back from the quaternion; a pure yaw comes back as (0, Yaw, 0).
	for (const FVector& Euler : CanonicalEulers)
	{
		const FVector Back =
			FLegacyCoordinateConversion::ToLegacyEulerXYZ(FLegacyCoordinateConversion::ConvertEulerXYZ(Euler));
		TestTrue(*FString::Printf("Euler (%g, %g, %g) round-trips as (%g, %g, %g)", Euler.X, Euler.Y, Euler.Z, Back.X,
					 Back.Y, Back.Z),
			Back.Equals(Euler, 1.0e-3f));
	}

	// Any triple comes back as the same rotation, possibly written differently.
	const FVector Others[] = {FVector(170.0f, 20.0f, -150.0f), FVector(0.0f, 90.0f, 45.0f),
		FVector(-135.0f, 100.0f, 0.0f), FVector(200.0f, -30.0f, 400.0f)};
	for (const FVector& Euler : Others)
	{
		const FQuat Rotation = FLegacyCoordinateConversion::ConvertEulerXYZ(Euler);
		const FQuat Back =
			FLegacyCoordinateConversion::ConvertEulerXYZ(FLegacyCoordinateConversion::ToLegacyEulerXYZ(Rotation));
		MatricesMatch(*this, FString::Printf("Rotation (%g, %g, %g)", Euler.X, Euler.Y, Euler.Z),
			FQuatRotationMatrix(Back), FQuatRotationMatrix(Rotation), 1.0e-4f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyCoordinateConversionLightRotationTest,
	"System.RenderCore.LegacyCoordinateConversion.LightRotation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyCoordinateConversionLightRotationTest::RunTest(const FString& Parameters)
{
	// A light rotation shines where the legacy pitch / yaw formula pointed, and gives its pitch and yaw back.
	const float PitchYaw[][2] = {
		{60.3f, 142.1f}, {50.0f, -30.0f}, {45.0f, 90.0f}, {-20.0f, 170.0f}, {0.0f, 0.0f}, {89.0f, -120.0f}};
	for (const float (&Angles)[2] : PitchYaw)
	{
		const FQuat Rotation = FLegacyCoordinateConversion::ConvertLightRotation(Angles[0], Angles[1]);
		const FVector Direction = FLegacyCoordinateConversion::ToLegacyDirection(
			Rotation.RotateVector(FLegacyCoordinateConversion::LightForward()));
		const FVector Expected = LegacyLightDirection(Angles[0], Angles[1]);
		TestTrue(*FString::Printf("Direction for pitch %g yaw %g", Angles[0], Angles[1]),
			Direction.Equals(Expected, 1.0e-6f));

		float Pitch = 0.0f;
		float Yaw = 0.0f;
		FLegacyCoordinateConversion::ToLegacyLightRotation(Rotation, Pitch, Yaw);
		TestEqual(*FString::Printf("Pitch %g round-trips", Angles[0]), Pitch, Angles[0], 1.0e-2f);
		TestEqual(*FString::Printf("Yaw %g round-trips", Angles[1]), Yaw, Angles[1], 1.0e-2f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
