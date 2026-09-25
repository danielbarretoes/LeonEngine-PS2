#include "CoreMinimal.h"
#include "LegacyCoordinateConversion.h"
#include "Math/RandomStream.h"
#include "MeshData.h"
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

	/**
	 * The legacy model matrix, glm T * Rx * Ry * Rz * S, written as FMatrix products (applied left to right), with the
	 * translation in centimetres (100 per legacy metre), still in the legacy (Y-up) axes.
	 */
	FMatrix LegacyModelMatrix(const FVector& Position, const FVector& EulerDegrees, const FVector& Scale)
	{
		return FScaleMatrix(Scale) * LegacyAxisRotation(2, EulerDegrees.Z) * LegacyAxisRotation(1, EulerDegrees.Y) *
			LegacyAxisRotation(0, EulerDegrees.X) * FTranslationMatrix(Position * 100.0f);
	}

	/** Row vectors: a legacy-axes point times this is the engine point (Y and Z swapped); it is its own inverse. */
	const FMatrix SwapYZ(FPlane(1.0f, 0.0f, 0.0f, 0.0f), FPlane(0.0f, 0.0f, 1.0f, 0.0f), FPlane(0.0f, 1.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f));

	/** A legacy-axes matrix in the engine basis: the same transform of the physical scene. */
	FMatrix ToEngineBasis(const FMatrix& LegacyMatrix)
	{
		return SwapYZ * LegacyMatrix * SwapYZ;
	}

	FVector RandomVector(FRandomStream& Random, float Range)
	{
		return FVector(
			Random.FRandRange(-Range, Range), Random.FRandRange(-Range, Range), Random.FRandRange(-Range, Range));
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
	// UE = 100 * (X, Z, Y): legacy Y and Z swap and a metre is 100 units. A default legacy transform is the identity,
	// and each conversion undoes its inverse.
	TestEqual("Units per metre", FLegacyCoordinateConversion::UnitsPerMetre, 100.0f);
	TestTrue("Position in cm, Y and Z swapped",
		FLegacyCoordinateConversion::ConvertPosition(FVector(1.0f, -2.0f, 0.5f))
			.Equals(FVector(100.0f, 50.0f, -200.0f), 1.0e-4f));
	TestEqual("Length in cm", FLegacyCoordinateConversion::ConvertLength(0.35f), 35.0f, 1.0e-4f);
	TestTrue("Legacy up is +Z",
		FLegacyCoordinateConversion::ConvertDirection(FVector(0.0f, 1.0f, 0.0f))
			.Equals(FVector(0.0f, 0.0f, 1.0f), 1.0e-6f));
	TestTrue("Legacy +Z is +Y",
		FLegacyCoordinateConversion::ConvertDirection(FVector(0.0f, 0.0f, 1.0f))
			.Equals(FVector(0.0f, 1.0f, 0.0f), 1.0e-6f));
	TestTrue("Scale swapped",
		FLegacyCoordinateConversion::ConvertScale(FVector(2.0f, 1.0f, 0.5f))
			.Equals(FVector(2.0f, 0.5f, 1.0f), 1.0e-6f));
	TestTrue("Extent in cm, swapped",
		FLegacyCoordinateConversion::ConvertExtent(FVector(0.25f, 1.0f, 4.0f))
			.Equals(FVector(25.0f, 400.0f, 100.0f), 1.0e-4f));
	const FVector4 Tangent = FLegacyCoordinateConversion::ConvertTangent(FVector4(0.6f, 0.8f, 0.0f, 1.0f));
	TestTrue("Tangent swapped, bitangent sign flipped",
		Tangent.X == 0.6f && Tangent.Y == 0.0f && Tangent.Z == 0.8f && Tangent.W == -1.0f);
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
	TestTrue("Extent",
		FLegacyCoordinateConversion::ToLegacyExtent(FLegacyCoordinateConversion::ConvertExtent(Legacy))
			.Equals(Legacy, 1.0e-6f));
	const FVector4 LegacyTangent(0.0f, 0.6f, 0.8f, -1.0f);
	const FVector4 TangentBack =
		FLegacyCoordinateConversion::ToLegacyTangent(FLegacyCoordinateConversion::ConvertTangent(LegacyTangent));
	TestTrue("Tangent",
		TangentBack.X == LegacyTangent.X && TangentBack.Y == LegacyTangent.Y && TangentBack.Z == LegacyTangent.Z &&
			TangentBack.W == LegacyTangent.W);
	const FQuat LegacyRotation(FVector(0.6f, 0.0f, 0.8f), 0.7f);
	TestTrue("Rotation",
		FLegacyCoordinateConversion::ToLegacyRotation(FLegacyCoordinateConversion::ConvertRotation(LegacyRotation))
			.Equals(LegacyRotation, 1.0e-7f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyCoordinateConversionTranslationYawTest,
	"System.RenderCore.LegacyCoordinateConversion.TranslationYaw",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyCoordinateConversionTranslationYawTest::RunTest(const FString& Parameters)
{
	// The position lands in the translation row in cm, Y and Z swapped. 90 degrees about legacy Y (up) sent local +X
	// to legacy -Z, which is -Y: a yaw of -90 about Z.
	const FTransform Transform = FLegacyCoordinateConversion::ConvertTransform(
		FVector(2.0f, 3.0f, 4.0f), FVector(0.0f, 90.0f, 0.0f), FVector::OneVector);
	const FMatrix M = Transform.ToMatrixWithScale();
	TestEqual("Translation x", M.M[3][0], 200.0f);
	TestEqual("Translation y", M.M[3][1], 400.0f);
	TestEqual("Translation z", M.M[3][2], 300.0f);
	TestEqual("X axis x", M.M[0][0], 0.0f, 1.0e-6f);
	TestEqual("X axis y", M.M[0][1], -1.0f, 1.0e-6f);
	TestTrue("Local +X goes to -Y",
		Transform.TransformVector(FVector(1.0f, 0.0f, 0.0f)).Equals(FVector(0.0f, -1.0f, 0.0f), 1.0e-6f));
	TestEqual("A yaw of -90", Transform.Rotator().Yaw, -90.0f, 1.0e-3f);
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
	TestEqual("Legacy scale y kept (world z)", Transform.GetScale3D().Z, 1.0f);
	TestEqual("Legacy scale z keeps its sign (world y)", Transform.GetScale3D().Y, -1.0e-4f);
	const FMatrix Inverse = Transform.ToMatrixWithScale().Inverse();
	TestTrue("Normal matrix finite", FMath::IsFinite(Inverse.M[0][0]) && FMath::IsFinite(Inverse.M[1][1]));
	TestEqual("Normal matrix not the singular fallback", Inverse.M[0][0], 1.0e4f, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyCoordinateConversionModelMatrixTest,
	"System.RenderCore.LegacyCoordinateConversion.ModelMatrix",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyCoordinateConversionModelMatrixTest::RunTest(const FString& Parameters)
{
	// ConvertTransform builds the legacy model matrix T * Rx * Ry * Rz * S in the engine basis (Swap * L * Swap): glm's
	// floats (the translation row in cm), and the sin / cos formula.
	const float GlmTrs[16] = {0.707106709f, 1.1464467f, 1.47839785f, 0, -0.306186229f, 0.369599462f, -0.140165031f, 0,
		-1.06066012f, -0.530330062f, 0.918558598f, 0, 100, -200, 350, 1};
	FMatrix Glm;
	FMemory::Memcpy(&Glm.M[0][0], GlmTrs, sizeof(GlmTrs));
	MatricesMatch(*this, "glm translate / rotate / scale",
		FLegacyCoordinateConversion::ConvertTransform(
			FVector(1.0f, -2.0f, 3.5f), FVector(30.0f, -45.0f, 60.0f), FVector(2.0f, 0.5f, 1.5f))
			.ToMatrixWithScale(),
		ToEngineBasis(Glm), 1.0e-6f);

	const FVector Position(-4.0f, 0.25f, 7.5f);
	const FVector Scale(0.5f, 3.0f, 1.25f);
	constexpr float Tolerance = 3.0e-6f; // 1e-6 per unit of the largest scale
	for (const FVector& Euler : CanonicalEulers)
	{
		MatricesMatch(*this, FString::Printf("Euler (%g, %g, %g)", Euler.X, Euler.Y, Euler.Z),
			FLegacyCoordinateConversion::ConvertTransform(Position, Euler, Scale).ToMatrixWithScale(),
			ToEngineBasis(LegacyModelMatrix(Position, Euler, Scale)), Tolerance);
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
		// The world light shines along its forward axis.
		const FVector Direction = FLegacyCoordinateConversion::ToLegacyDirection(Rotation.GetForwardVector());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyCoordinateConversionRotationMatchesBasisTest,
	"System.RenderCore.LegacyCoordinateConversion.RotationMatchesBasis",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyCoordinateConversionRotationMatchesBasisTest::RunTest(const FString& Parameters)
{
	// For random legacy rotations R and vectors V: Convert(R V) == ConvertRotation(R) Convert(V). The swap has
	// determinant -1, so a cross product flips: Convert(A ^ B) == -(Convert(A) ^ Convert(B)) (Right = Up ^ Forward).
	FRandomStream Random(1234);
	for (int32 Index = 0; Index < 64; ++Index)
	{
		const FVector Axis = RandomVector(Random, 1.0f).GetSafeNormal();
		if (Axis.IsNearlyZero())
		{
			continue;
		}
		const FQuat Legacy(Axis, Random.FRandRange(-PI, PI));
		const FVector V = RandomVector(Random, 3.0f);
		const FVector Expected = FLegacyCoordinateConversion::ConvertPosition(Legacy.RotateVector(V));
		const FVector Actual = FLegacyCoordinateConversion::ConvertRotation(Legacy).RotateVector(
			FLegacyCoordinateConversion::ConvertPosition(V));
		if (!TestTrue(*FString::Printf("Rotation %d", Index), Actual.Equals(Expected, 1.0e-3f)))
		{
			return false;
		}
		const FVector A = RandomVector(Random, 1.0f);
		const FVector B = RandomVector(Random, 1.0f);
		const FVector CrossExpected = FLegacyCoordinateConversion::ConvertDirection(A ^ B);
		const FVector CrossActual =
			-(FLegacyCoordinateConversion::ConvertDirection(A) ^ FLegacyCoordinateConversion::ConvertDirection(B));
		if (!TestTrue(*FString::Printf("Cross product %d", Index), CrossActual.Equals(CrossExpected, 1.0e-5f)))
		{
			return false;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyCoordinateConversionAngleMapTest,
	"System.RenderCore.LegacyCoordinateConversion.AngleMap",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyCoordinateConversionAngleMapTest::RunTest(const FString& Parameters)
{
	// The legacy angles become UE rotations that point where the legacy formulas pointed, and come back.
	const float Angles[][2] = {{0.0f, 0.0f}, {30.0f, 20.0f}, {-120.0f, 45.0f}, {200.0f, -10.0f}, {-90.0f, -20.0f}};
	for (const float (&YawPitch)[2] : Angles)
	{
		const float Yaw = YawPitch[0];
		const float Pitch = YawPitch[1];
		const float Cy = FMath::Cos(FMath::DegreesToRadians(Yaw));
		const float Sy = FMath::Sin(FMath::DegreesToRadians(Yaw));
		const float Cp = FMath::Cos(FMath::DegreesToRadians(Pitch));
		const float Sp = FMath::Sin(FMath::DegreesToRadians(Pitch));
		const FString Label = FString::Printf("(%g, %g)", static_cast<double>(Yaw), static_cast<double>(Pitch));

		// Actor yaw: the legacy facing (sin Y, 0, cos Y) is the world yaw 90 - Y.
		const FVector ActorFacing = FLegacyCoordinateConversion::ConvertDirection(FVector(Sy, 0.0f, Cy));
		TestTrue(*("Actor yaw " + Label),
			FRotator(0.0f, FLegacyCoordinateConversion::ConvertActorYaw(Yaw), 0.0f)
				.Vector()
				.Equals(ActorFacing, 1.0e-5f));
		TestTrue(*("Actor record " + Label),
			FLegacyCoordinateConversion::ConvertActorEulerXYZ(FVector(0.0f, Yaw, 0.0f))
				.GetForwardVector()
				.Equals(ActorFacing, 1.0e-5f));
		TestEqual(*("Actor yaw back " + Label),
			FLegacyCoordinateConversion::ToLegacyActorYaw(FLegacyCoordinateConversion::ConvertActorYaw(Yaw)),
			FRotator::NormalizeAxis(Yaw), 1.0e-3f);
		// (Away from the Euler gimbal lock at a legacy yaw of +-90, where X and Z turn about the same axis.)
		if (FMath::Abs(Cy) > 0.1f)
		{
			const FVector ActorEuler(10.0f, Yaw, -20.0f);
			TestTrue(*("Actor record back " + Label),
				FLegacyCoordinateConversion::ToLegacyActorEulerXYZ(
					FLegacyCoordinateConversion::ConvertActorEulerXYZ(ActorEuler))
					.Equals(FVector(10.0f, FRotator::NormalizeAxis(Yaw), -20.0f), 1.0e-3f));
		}

		// Orbit: the legacy eye offset (cos P cos Y, sin P, cos P sin Y) is minus the view direction.
		const FRotator Orbit = FLegacyCoordinateConversion::ConvertOrbitViewRotation(Yaw, Pitch);
		const FVector EyeOffset = FLegacyCoordinateConversion::ConvertDirection(FVector(Cp * Cy, Sp, Cp * Sy));
		TestTrue(*("Orbit " + Label), (-Orbit.Vector()).Equals(EyeOffset, 1.0e-5f));
		float BackYaw = 0.0f;
		float BackPitch = 0.0f;
		FLegacyCoordinateConversion::ToLegacyOrbitRotation(Orbit, BackYaw, BackPitch);
		TestEqual(*("Orbit yaw back " + Label), BackYaw, FRotator::NormalizeAxis(Yaw), 1.0e-3f);
		TestEqual(*("Orbit pitch back " + Label), BackPitch, Pitch, 1.0e-3f);

		// Free look: the legacy forward (cos P cos Y, sin P, cos P sin Y) is the view direction.
		const FRotator FreeLook = FLegacyCoordinateConversion::ConvertFreeLookRotation(Yaw, Pitch);
		TestTrue(*("Free look " + Label), FreeLook.Vector().Equals(EyeOffset, 1.0e-5f));
		FLegacyCoordinateConversion::ToLegacyFreeLookRotation(FreeLook, BackYaw, BackPitch);
		TestEqual(*("Free look yaw back " + Label), BackYaw, FRotator::NormalizeAxis(Yaw), 1.0e-3f);
		TestEqual(*("Free look pitch back " + Label), BackPitch, Pitch, 1.0e-3f);
	}
	TestEqual("Yaw rate", FLegacyCoordinateConversion::ConvertYawRate(30.0f), -30.0f);
	TestEqual("Yaw rate back", FLegacyCoordinateConversion::ToLegacyYawRate(-30.0f), 30.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyCoordinateConversionMeshDataTest,
	"System.RenderCore.LegacyCoordinateConversion.MeshData",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyCoordinateConversionMeshDataTest::RunTest(const FString& Parameters)
{
	// A converted legacy vertex keeps its tangent frame: the shader's bitangent cross(N, T) * w is the converted legacy
	// bitangent, and the index order is kept.
	FMeshData Data;
	const FVector Normal(0.0f, 0.6f, 0.8f);
	const FVector Tangent(1.0f, 0.0f, 0.0f);
	Data.Vertices.Add(FVertex(FVector(1.0f, 2.0f, 3.0f), Normal, FVector2D(0.25f, 0.5f), FVector4(Tangent, -1.0f)));
	Data.Indices = {0, 0, 0};
	const FVector LegacyBitangent = (Normal ^ Tangent) * -1.0f;

	FLegacyCoordinateConversion::ConvertMeshData(Data);
	const FVertex& Vertex = Data.Vertices[0];
	TestTrue("Position", Vertex.Position.Equals(FVector(100.0f, 300.0f, 200.0f), 1.0e-4f));
	TestTrue("Normal", Vertex.Normal.Equals(FVector(0.0f, 0.8f, 0.6f), 1.0e-6f));
	TestTrue("UV kept", Vertex.TexCoord.X == 0.25f && Vertex.TexCoord.Y == 0.5f);
	const FVector Bitangent = (Vertex.Normal ^ FVector(Vertex.Tangent)) * Vertex.Tangent.W;
	TestTrue("Bitangent", Bitangent.Equals(FLegacyCoordinateConversion::ConvertDirection(LegacyBitangent), 1.0e-6f));
	TestTrue("Indices kept", Data.Indices[0] == 0 && Data.Indices[2] == 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
