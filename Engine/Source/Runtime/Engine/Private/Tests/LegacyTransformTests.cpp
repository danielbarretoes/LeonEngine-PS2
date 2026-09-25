#include "CoreMinimal.h"
#include "LegacyGLMath.h"
#include "Level/LegacyTransform.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// FLegacyTransform matrices use the GL layout: glm column c, row r is M.M[c][r].

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyTransformIdentityTest, "System.Engine.LegacyTransform.Identity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyTransformIdentityTest::RunTest(const FString& Parameters)
{
	// A default transform builds the identity.
	const FLegacyTransform Transform;
	const FMatrix M = Transform.ModelMatrix();
	TestEqual("M[0][0]", M.M[0][0], 1.0f);
	TestEqual("M[1][1]", M.M[1][1], 1.0f);
	TestEqual("M[2][2]", M.M[2][2], 1.0f);
	TestEqual("Translation x", M.M[3][0], 0.0f);
	TestEqual("Translation y", M.M[3][1], 0.0f);
	TestEqual("Translation z", M.M[3][2], 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyTransformTranslationYawTest, "System.Engine.LegacyTransform.TranslationYaw",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyTransformTranslationYawTest::RunTest(const FString& Parameters)
{
	// Position lands in column 3; 90 degrees of yaw sends local +X to world -Z.
	FLegacyTransform Transform;
	Transform.Position = FVector(2.0f, 3.0f, 4.0f);
	Transform.RotationDegrees = FVector(0.0f, 90.0f, 0.0f);
	const FMatrix M = Transform.ModelMatrix();
	TestEqual("Translation x", M.M[3][0], 2.0f);
	TestEqual("Translation y", M.M[3][1], 3.0f);
	TestEqual("Translation z", M.M[3][2], 4.0f);
	TestEqual("X axis x", M.M[0][0], 0.0f);
	TestEqual("X axis z", M.M[0][2], -1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyTransformZeroScaleTest, "System.Engine.LegacyTransform.ZeroScale",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyTransformZeroScaleTest::RunTest(const FString& Parameters)
{
	// A zero scale is kept away from zero, so the normal matrix stays finite.
	FLegacyTransform Transform;
	Transform.Scale = FVector(0.0f, 1.0f, 0.0f);
	const FMatrix M = Transform.ModelMatrix();
	TestTrue("Scale x sanitised", FMath::Abs(M.M[0][0]) >= 1.0e-4f);
	TestTrue("Scale z sanitised", FMath::Abs(M.M[2][2]) >= 1.0e-4f);
	float Normal[9];
	LegacyGL::NormalMatrix3x3(M, Normal);
	TestTrue("Normal matrix finite", FMath::IsFinite(Normal[0]) && FMath::IsFinite(Normal[4]));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
