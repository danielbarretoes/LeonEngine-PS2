#include "CoreMinimal.h"
#include "LegacyGLMath.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Expected values were printed by glm 1.0.1 (the library LegacyGLMath replaces) before it left the repo, as the 16
// floats of each matrix in memory order (glm column c, row r at index c * 4 + r).
namespace
{
	bool MatrixMatches(FAutomationTestBase& Test, const TCHAR* What, const FMatrix& Actual, const float (&Expected)[16])
	{
		for (int32 Index = 0; Index < 16; ++Index)
		{
			const float Value = Actual.M[Index / 4][Index % 4];
			if (FMath::Abs(Value - Expected[Index]) > 1.0e-5f * FMath::Max(1.0f, FMath::Abs(Expected[Index])))
			{
				Test.AddError(FString::Printf("%s: element %d is %.9g, glm gives %.9g", What, Index,
					static_cast<double>(Value), static_cast<double>(Expected[Index])));
				return false;
			}
		}
		return true;
	}

	/**
	 * The transform shared by both tests, glm::translate(1, -2, 3.5) * rotate X 30 * rotate Y -45 * rotate Z 60 *
	 * scale(2, 0.5, 1.5): as FMatrix products it reads in the order the transforms apply.
	 */
	FMatrix MakeTrs()
	{
		const FQuat RotX(FVector(1.0f, 0.0f, 0.0f), FMath::DegreesToRadians(30.0f));
		const FQuat RotY(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(-45.0f));
		const FQuat RotZ(FVector(0.0f, 0.0f, 1.0f), FMath::DegreesToRadians(60.0f));
		return FScaleMatrix(FVector(2.0f, 0.5f, 1.5f)) * FQuatRotationMatrix(RotZ) * FQuatRotationMatrix(RotY) *
			FQuatRotationMatrix(RotX) * FTranslationMatrix(FVector(1.0f, -2.0f, 3.5f));
	}

	FMatrix MakeLookAt()
	{
		return LegacyGL::LookAt(FVector(3.0f, 4.0f, 5.0f), FVector(0.5f, 1.0f, -2.0f), FVector(0.0f, 1.0f, 0.0f));
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyGLMathBuildersTest, "System.RenderCore.LegacyGLMath.Builders",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyGLMathBuildersTest::RunTest(const FString& Parameters)
{
	// Perspective, Ortho, LookAt and QuatToMatrix build glm's matrices; FMatrix products build glm's translate /
	// rotate / scale chain.
	const float Perspective[16] = {
		0.974278629f, 0, 0, 0, 0, 1.7320509f, 0, 0, 0, 0, -1.002002f, -1, 0, 0, -0.2002002f, 0};
	MatrixMatches(*this, "Perspective",
		LegacyGL::Perspective(FMath::DegreesToRadians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f), Perspective);

	const float Ortho[16] = {0.200000003f, 0, 0, 0, 0, 0.400000006f, 0, 0, 0, 0, -0.0404040404f, 0, -0.200000003f,
		-0.200000003f, -1.02020204f, 1};
	MatrixMatches(*this, "Ortho", LegacyGL::Ortho(-4.0f, 6.0f, -2.0f, 3.0f, 0.5f, 50.0f), Ortho);

	const float LookAt[16] = {0.941741824f, -0.125880525f, 0.311891437f, 0, 0, 0.927319825f, 0.374269724f, 0,
		-0.336336374f, -0.352465451f, 0.873296022f, 0, -1.14354348f, -1.56931067f, -6.79923296f, 1};
	MatrixMatches(*this, "LookAt", MakeLookAt(), LookAt);

	const float Trs[16] = {0.707106709f, 1.1464467f, 1.47839785f, 0, -0.306186229f, 0.369599462f, -0.140165031f, 0,
		-1.06066012f, -0.530330062f, 0.918558598f, 0, 1, -2, 3.5f, 1};
	MatrixMatches(*this, "Translate Rotate Scale", MakeTrs(), Trs);

	const float Quat[16] = {0.359999955f, 0.480000019f, 0.800000072f, 0, -0.800000072f, 0.599999964f, 0, 0,
		-0.480000019f, -0.640000045f, 0.599999964f, 0, 0, 0, 0, 1};
	MatrixMatches(*this, "QuatToMatrix", LegacyGL::QuatToMatrix(0.8f, 0.2f, -0.4f, 0.4f), Quat);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyGLMathCompositionTest, "System.RenderCore.LegacyGLMath.Composition",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyGLMathCompositionTest::RunTest(const FString& Parameters)
{
	// FMatrix products, TransformPosition, NormalMatrix3x3, GetUnsafeNormal and DegreesToRadians give what glm's
	// operators gave.
	const FMatrix A = MakeTrs();
	const float Mul[16] = {0.373644024f, 0.867726088f, 1.69640374f, 0, -0.680905581f, 0.144250408f, 0.213810876f, 0,
		-1.05617595f, -0.978997886f, 0.354337931f, 0, 7.88356924f, -0.285190463f, -4.21614361f, 1};
	// glm's A * B is FMatrix's B * A: the same 16 floats, read as row vectors.
	MatrixMatches(*this, "Product (glm A * B)", MakeLookAt() * A, Mul);

	const FVector Point = A.TransformPosition(FVector(1.0f, 2.0f, 3.0f));
	TestTrue("TransformPoint", Point.Equals(FVector(-2.08724618f, -1.70534444f, 7.45374346f), 1.0e-5f));

	float Normal[9];
	LegacyGL::NormalMatrix3x3(A, Normal);
	const float ExpectedNormal[9] = {0.176776692f, 0.286611646f, 0.369599462f, -1.22474504f, 1.47839773f, -0.560660243f,
		-0.471404523f, -0.235702276f, 0.408248305f};
	for (int32 Index = 0; Index < 9; ++Index)
	{
		TestEqual(*FString::Printf("Normal matrix element %d", Index), Normal[Index], ExpectedNormal[Index], 1.0e-5f);
	}

	TestTrue("Normalize",
		FVector(3.0f, -4.0f, 12.0f)
			.GetUnsafeNormal()
			.Equals(FVector(0.230769247f, -0.307692319f, 0.923076987f), 1.0e-7f));
	TestEqual("DegreesToRadians", FMath::DegreesToRadians(37.5f), 0.654498458f, 1.0e-7f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
