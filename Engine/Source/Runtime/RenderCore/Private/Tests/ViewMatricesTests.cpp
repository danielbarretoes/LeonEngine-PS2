#include "CoreMinimal.h"
#include "GLClipSpace.h"
#include "Misc/AutomationTest.h"
#include "ViewMatrices.h"

#if WITH_DEV_AUTOMATION_TESTS

// Reference values were printed by glm 1.0.1 (the renderer's math library before P7), as the 16 floats of each matrix
// in memory order (glm column c, row r at index c * 4 + r). The same floats read as an FMatrix are the row-vector
// matrix of the same transform.
namespace
{
	/** glm::perspective(radians(60), 16 / 9, 0.1, 100): GL view (looking down -Z) to GL clip space. */
	constexpr float GlmPerspective[16] = {
		0.974278629f, 0, 0, 0, 0, 1.7320509f, 0, 0, 0, 0, -1.002002f, -1, 0, 0, -0.2002002f, 0};

	/** glm::ortho(-4, 6, -2, 3, 0.5, 50). */
	constexpr float GlmOrtho[16] = {0.200000003f, 0, 0, 0, 0, 0.400000006f, 0, 0, 0, 0, -0.0404040404f, 0,
		-0.200000003f, -0.200000003f, -1.02020204f, 1};

	/** glm::lookAt((3, 4, 5), (0.5, 1, -2), (0, 1, 0)): world to GL view space. */
	constexpr float GlmLookAt[16] = {0.941741824f, -0.125880525f, 0.311891437f, 0, 0, 0.927319825f, 0.374269724f, 0,
		-0.336336374f, -0.352465451f, 0.873296022f, 0, -1.14354348f, -1.56931067f, -6.79923296f, 1};

	const FVector LookAtEye(3.0f, 4.0f, 5.0f);
	const FVector LookAtTarget(0.5f, 1.0f, -2.0f);
	const FVector WorldUp(0.0f, 1.0f, 0.0f);

	FMatrix FromGlm(const float (&Floats)[16])
	{
		FMatrix Result;
		FMemory::Memcpy(&Result.M[0][0], Floats, sizeof(Floats));
		return Result;
	}

	/**
	 * A glm matrix with the given row (row-vector input coordinate) or column (output coordinate) negated: UE view
	 * space is GL view space mirrored in z.
	 */
	FMatrix NegateRow(FMatrix M, int32 Row)
	{
		for (int32 Col = 0; Col < 4; ++Col)
		{
			M.M[Row][Col] = -M.M[Row][Col];
		}
		return M;
	}

	FMatrix NegateColumn(FMatrix M, int32 Col)
	{
		for (int32 Row = 0; Row < 4; ++Row)
		{
			M.M[Row][Col] = -M.M[Row][Col];
		}
		return M;
	}

	bool MatrixMatches(FAutomationTestBase& Test, const TCHAR* What, const FMatrix& Actual, const FMatrix& Expected)
	{
		for (int32 Index = 0; Index < 16; ++Index)
		{
			const float Value = Actual.M[Index / 4][Index % 4];
			const float Reference = Expected.M[Index / 4][Index % 4];
			if (FMath::Abs(Value - Reference) > 1.0e-5f * FMath::Max(1.0f, FMath::Abs(Reference)))
			{
				Test.AddError(FString::Printf("%s: element [%d][%d] is %.9g, expected %.9g", What, Index / 4, Index % 4,
					static_cast<double>(Value), static_cast<double>(Reference)));
				return false;
			}
		}
		return true;
	}

	FVector Ndc(const FMatrix& ViewProjection, const FVector& Point)
	{
		const FVector4 Clip = ViewProjection.TransformFVector4(FVector4(Point, 1.0f));
		return FVector(Clip.X / Clip.W, Clip.Y / Clip.W, Clip.Z / Clip.W);
	}

	/** The UE perspective with glm::perspective's vertical field of view, as UCameraComponent builds it. */
	FMatrix MakeUEPerspective(float FovYDegrees, float Aspect, float ZNear, float ZFar)
	{
		const float HalfFov = FMath::DegreesToRadians(FovYDegrees) / 2.0f;
		return FPerspectiveMatrix(HalfFov, HalfFov, 1.0f / Aspect, 1.0f, ZNear, ZFar);
	}

	/**
	 * glm::translate(1, -2, 3.5) * rotate X 30 * rotate Y -45 * rotate Z 60 * scale(2, 0.5, 1.5): as FMatrix products
	 * it reads in the order the transforms apply.
	 */
	FMatrix MakeTrs()
	{
		const FQuat RotX(FVector(1.0f, 0.0f, 0.0f), FMath::DegreesToRadians(30.0f));
		const FQuat RotY(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(-45.0f));
		const FQuat RotZ(FVector(0.0f, 0.0f, 1.0f), FMath::DegreesToRadians(60.0f));
		return FScaleMatrix(FVector(2.0f, 0.5f, 1.5f)) * FQuatRotationMatrix(RotZ) * FQuatRotationMatrix(RotY) *
			FQuatRotationMatrix(RotX) * FTranslationMatrix(FVector(1.0f, -2.0f, 3.5f));
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FViewMatricesLookAtBasisTest, "System.RenderCore.ViewMatrices.LookAtBasis",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FViewMatricesLookAtBasisTest::RunTest(const FString& Parameters)
{
	// UE view space: a point to the camera's screen-right has x > 0, one above has y > 0, one in front has z > 0.
	// The camera looks down -Z with Y up, so its screen-right is +X in the right-handed world.
	const FMatrix Default = MakeLookAtView(FVector::ZeroVector, FVector(0.0f, 0.0f, -1.0f), WorldUp);
	TestTrue("Right is +x",
		FVector(Default.TransformPosition(FVector(1.0f, 0.0f, -5.0f))).Equals(FVector(1.0f, 0.0f, 5.0f)));
	TestTrue(
		"Up is +y", FVector(Default.TransformPosition(FVector(0.0f, 1.0f, -5.0f))).Equals(FVector(0.0f, 1.0f, 5.0f)));
	TestTrue("Forward is +z",
		FVector(Default.TransformPosition(FVector(0.0f, 0.0f, -5.0f))).Equals(FVector(0.0f, 0.0f, 5.0f)));

	// A general camera: the view axes are Right = Forward ^ Up, Up and Forward, and the space is left-handed.
	const FMatrix View = MakeLookAtView(LookAtEye, LookAtTarget, WorldUp);
	const FVector Forward = (LookAtTarget - LookAtEye).GetUnsafeNormal();
	const FVector Right = (Forward ^ WorldUp).GetUnsafeNormal();
	const FVector Up = Right ^ Forward;
	const FVector Point = LookAtEye + (Right * 1.5f) + (Up * 0.5f) + (Forward * 4.0f);
	TestTrue("View position", FVector(View.TransformPosition(Point)).Equals(FVector(1.5f, 0.5f, 4.0f), 1.0e-5f));
	TestTrue("Eye at the origin", FVector(View.TransformPosition(LookAtEye)).Equals(FVector::ZeroVector, 1.0e-5f));
	TestEqual("Left-handed view (determinant -1)", View.RotDeterminant(), -1.0f, 1.0e-5f);

	// It is glm's right-handed lookAt mirrored in z.
	MatrixMatches(*this, "glm lookAt mirrored in z", View, NegateColumn(FromGlm(GlmLookAt), 2));

	// MakeViewMatrix from the same basis gives the same matrix.
	MatrixMatches(*this, "MakeViewMatrix", MakeViewMatrix(LookAtEye, Forward, Right, Up), View);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FViewMatricesGLClipSpaceMatchesGlmPerspectiveTest,
	"System.RenderCore.ViewMatrices.GLClipSpaceMatchesGlmPerspective",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FViewMatricesGLClipSpaceMatchesGlmPerspectiveTest::RunTest(const FString& Parameters)
{
	// UE's perspective maps depth to [0, 1], near to 0.
	const FMatrix Projection = MakeUEPerspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
	TestEqual("UE near depth", Ndc(Projection, FVector(0.0f, 0.0f, 0.1f)).Z, 0.0f, 1.0e-6f);
	TestEqual("UE far depth", Ndc(Projection, FVector(0.0f, 0.0f, 100.0f)).Z, 1.0f, 1.0e-6f);

	// Through the adapter, GL's [-1, 1]; the matrix is glm's perspective for a view mirrored in z.
	const FMatrix ProjectionGL = ToGLClipSpace(Projection);
	TestEqual("GL near depth", Ndc(ProjectionGL, FVector(0.0f, 0.0f, 0.1f)).Z, -1.0f, 1.0e-5f);
	TestEqual("GL far depth", Ndc(ProjectionGL, FVector(0.0f, 0.0f, 100.0f)).Z, 1.0f, 1.0e-5f);
	MatrixMatches(
		*this, "glm perspective with the view mirrored in z", ProjectionGL, NegateRow(FromGlm(GlmPerspective), 2));

	// The UE look-at view through it gives the NDC of glm's lookAt * perspective.
	const FMatrix ViewProjection = MakeLookAtView(LookAtEye, LookAtTarget, WorldUp) * ProjectionGL;
	const FMatrix GlmViewProjection = FromGlm(GlmLookAt) * FromGlm(GlmPerspective);
	const FVector Points[4] = {FVector(0.5f, 1.0f, -2.0f), FVector(-1.0f, 2.0f, 0.0f), FVector(2.0f, 0.0f, -6.0f),
		FVector(4.0f, 3.5f, -10.0f)};
	for (const FVector& Point : Points)
	{
		const FVector Actual = Ndc(ViewProjection, Point);
		const FVector Expected = Ndc(GlmViewProjection, Point);
		TestTrue(*FString::Printf("NDC of (%g, %g, %g)", static_cast<double>(Point.X), static_cast<double>(Point.Y),
					 static_cast<double>(Point.Z)),
			Actual.Equals(Expected, 1.0e-5f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FViewMatricesGLClipSpaceMatchesGlmOrthoTest,
	"System.RenderCore.ViewMatrices.GLClipSpaceMatchesGlmOrtho",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FViewMatricesGLClipSpaceMatchesGlmOrthoTest::RunTest(const FString& Parameters)
{
	// An off-centre box as the shadow fit builds it: centre x / y, a UE ortho with half sizes and depth [0, 1] from
	// near to far, then GL clip space. It is glm's ortho for a view mirrored in z.
	const float Left = -4.0f;
	const float Right = 6.0f;
	const float Bottom = -2.0f;
	const float Top = 3.0f;
	const float ZNear = 0.5f;
	const float ZFar = 50.0f;
	const FMatrix Ortho = FTranslationMatrix(FVector(-(Left + Right) * 0.5f, -(Bottom + Top) * 0.5f, 0.0f)) *
		FOrthoMatrix((Right - Left) * 0.5f, (Top - Bottom) * 0.5f, 1.0f / (ZFar - ZNear), -ZNear);
	TestEqual("UE near depth", Ndc(Ortho, FVector(0.0f, 0.0f, ZNear)).Z, 0.0f, 1.0e-6f);
	TestEqual("UE far depth", Ndc(Ortho, FVector(0.0f, 0.0f, ZFar)).Z, 1.0f, 1.0e-6f);
	MatrixMatches(
		*this, "glm ortho with the view mirrored in z", ToGLClipSpace(Ortho), NegateRow(FromGlm(GlmOrtho), 2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FViewMatricesGlmModelMatricesTest, "System.RenderCore.ViewMatrices.GlmModelMatrices",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FViewMatricesGlmModelMatricesTest::RunTest(const FString& Parameters)
{
	// FMatrix products build glm's translate / rotate / scale chain, and FQuatRotationMatrix glm::mat4_cast (the FBX
	// skeletal importer's joint rotations); products, TransformPosition, GetUnsafeNormal and DegreesToRadians give what
	// glm's operators gave.
	const float Trs[16] = {0.707106709f, 1.1464467f, 1.47839785f, 0, -0.306186229f, 0.369599462f, -0.140165031f, 0,
		-1.06066012f, -0.530330062f, 0.918558598f, 0, 1, -2, 3.5f, 1};
	const FMatrix A = MakeTrs();
	MatrixMatches(*this, "Translate Rotate Scale", A, FromGlm(Trs));

	// glm::mat4_cast(quat(w = 0.8, x = 0.2, y = -0.4, z = 0.4)).
	const float Quat[16] = {0.359999955f, 0.480000019f, 0.800000072f, 0, -0.800000072f, 0.599999964f, 0, 0,
		-0.480000019f, -0.640000045f, 0.599999964f, 0, 0, 0, 0, 1};
	MatrixMatches(*this, "Quaternion rotation", FQuatRotationMatrix(FQuat(0.2f, -0.4f, 0.4f, 0.8f)), FromGlm(Quat));

	// glm's lookAt * A is FMatrix's A * lookAt: the same 16 floats, read as row vectors.
	const float Mul[16] = {0.373644024f, 0.867726088f, 1.69640374f, 0, -0.680905581f, 0.144250408f, 0.213810876f, 0,
		-1.05617595f, -0.978997886f, 0.354337931f, 0, 7.88356924f, -0.285190463f, -4.21614361f, 1};
	MatrixMatches(*this, "Product (glm A * B)", FromGlm(GlmLookAt) * A, FromGlm(Mul));

	const FVector Point = A.TransformPosition(FVector(1.0f, 2.0f, 3.0f));
	TestTrue("TransformPoint", Point.Equals(FVector(-2.08724618f, -1.70534444f, 7.45374346f), 1.0e-5f));
	TestTrue("Normalize",
		FVector(3.0f, -4.0f, 12.0f)
			.GetUnsafeNormal()
			.Equals(FVector(0.230769247f, -0.307692319f, 0.923076987f), 1.0e-7f));
	TestEqual("DegreesToRadians", FMath::DegreesToRadians(37.5f), 0.654498458f, 1.0e-7f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
