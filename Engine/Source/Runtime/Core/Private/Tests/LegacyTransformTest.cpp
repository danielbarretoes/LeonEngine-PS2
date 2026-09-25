#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

// FLegacyTransform (glm) is desktop only; it goes away in P6.
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_DESKTOP

	#include "Migration/LegacyTransform.h"

	#include <cmath>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyTransformIdentityTest, "System.Core.Migration.LegacyTransform.Identity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyTransformIdentityTest::RunTest(const FString& Parameters)
{
	const FLegacyTransform Transform;
	const glm::mat4 M = Transform.ModelMatrix();
	TestEqual("M[0][0]", M[0][0], 1.0f);
	TestEqual("M[1][1]", M[1][1], 1.0f);
	TestEqual("M[2][2]", M[2][2], 1.0f);
	TestEqual("Translation x", M[3][0], 0.0f);
	TestEqual("Translation y", M[3][1], 0.0f);
	TestEqual("Translation z", M[3][2], 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyTransformTranslationYawTest,
	"System.Core.Migration.LegacyTransform.TranslationYaw",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyTransformTranslationYawTest::RunTest(const FString& Parameters)
{
	FLegacyTransform Transform;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyTransformZeroScaleTest, "System.Core.Migration.LegacyTransform.ZeroScale",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyTransformZeroScaleTest::RunTest(const FString& Parameters)
{
	FLegacyTransform Transform;
	Transform.Scale = {0.0f, 1.0f, 0.0f};
	const glm::mat4 M = Transform.ModelMatrix();
	TestTrue("Scale x sanitised", std::abs(M[0][0]) >= 1.0e-4f);
	TestTrue("Scale z sanitised", std::abs(M[2][2]) >= 1.0e-4f);
	const glm::mat3 N = Transform.NormalMatrix();
	TestTrue("Normal matrix finite", std::isfinite(N[0][0]) && std::isfinite(N[1][1]));
	return true;
}

#endif
