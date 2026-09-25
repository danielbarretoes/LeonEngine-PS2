#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

// The glm-based FTransform is desktop only until Core math replaces it.
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_DESKTOP

	#include "Math/Transform.h"

	#include <cmath>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransformIdentityTest, "System.Core.Math.Transform.Identity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTransformIdentityTest::RunTest(const FString& Parameters)
{
	const FTransform Transform;
	const glm::mat4 M = Transform.ModelMatrix();
	TestEqual("M[0][0]", M[0][0], 1.0f);
	TestEqual("M[1][1]", M[1][1], 1.0f);
	TestEqual("M[2][2]", M[2][2], 1.0f);
	TestEqual("Translation x", M[3][0], 0.0f);
	TestEqual("Translation y", M[3][1], 0.0f);
	TestEqual("Translation z", M[3][2], 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransformTranslationYawTest, "System.Core.Math.Transform.TranslationYaw",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTransformTranslationYawTest::RunTest(const FString& Parameters)
{
	FTransform Transform;
	Transform.Position = {2.0f, 3.0f, 4.0f};
	Transform.RotationDegrees = {0.0f, 90.0f, 0.0f};
	const glm::mat4 M = Transform.ModelMatrix();
	TestEqual("Translation x", M[3].x, 2.0f);
	TestEqual("Translation y", M[3].y, 3.0f);
	TestEqual("Translation z", M[3].z, 4.0f);
	// 90 degrees of yaw: local +X goes to world -Z.
	TestEqual("X axis x", M[0].x, 0.0f);
	TestEqual("X axis z", M[0].z, -1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransformZeroScaleTest, "System.Core.Math.Transform.ZeroScale",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FTransformZeroScaleTest::RunTest(const FString& Parameters)
{
	FTransform Transform;
	Transform.Scale = {0.0f, 1.0f, 0.0f};
	const glm::mat4 M = Transform.ModelMatrix();
	TestTrue("Scale x sanitised", std::abs(M[0][0]) >= 1.0e-4f);
	TestTrue("Scale z sanitised", std::abs(M[2][2]) >= 1.0e-4f);
	const glm::mat3 N = Transform.NormalMatrix();
	TestTrue("Normal matrix finite", std::isfinite(N[0][0]) && std::isfinite(N[1][1]));
	return true;
}

#endif
