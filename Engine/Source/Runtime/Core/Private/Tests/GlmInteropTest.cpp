#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

// Core math checked against glm, the library it replaces. Desktop only, like glm; goes away with it in P6.
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_DESKTOP

	#include "Migration/GlmInterop.h"

	#include <glm/gtc/matrix_transform.hpp>
	#include <glm/gtc/quaternion.hpp>

namespace
{
	bool MatrixMatchesGlm(
		FAutomationTestBase& Test, const TCHAR* What, const FMatrix& Actual, const glm::mat4& Expected)
	{
		if (!Actual.Equals(FromGlm(Expected), 1.e-5f))
		{
			Test.AddError(
				FString::Printf("%s: expected %s, got %s", What, *FromGlm(Expected).ToString(), *Actual.ToString()));
			return false;
		}
		return true;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGlmInteropMatrixTest, "System.Core.Migration.GlmInterop.Matrix",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGlmInteropMatrixTest::RunTest(const FString& Parameters)
{
	// Same transform, same 16 floats: row vectors with the matrix on the right are glm's column vectors transposed.
	MatrixMatchesGlm(*this, "Translation", FTranslationMatrix(FVector(1, 2, 3)),
		glm::translate(glm::mat4(1.0f), glm::vec3(1, 2, 3)));
	MatrixMatchesGlm(*this, "Scale", FScaleMatrix(FVector(2, 3, 4)), glm::scale(glm::mat4(1.0f), glm::vec3(2, 3, 4)));

	// UE yaw is a right-handed turn about Z; pitch and roll turn the other way about Y and X. Roll is applied first.
	const glm::mat4 Yaw = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0, 0, 1));
	const glm::mat4 Pitch = glm::rotate(glm::mat4(1.0f), glm::radians(-30.0f), glm::vec3(0, 1, 0));
	const glm::mat4 Roll = glm::rotate(glm::mat4(1.0f), glm::radians(-60.0f), glm::vec3(1, 0, 0));
	MatrixMatchesGlm(*this, "Rotation", FRotationMatrix(FRotator(30.f, 45.f, 60.f)), Yaw * Pitch * Roll);

	// A * B applies A first; glm's B * A does the same.
	const FMatrix A = FScaleRotationTranslationMatrix(FVector(2, 1, 1), FRotator(10.f, 20.f, 30.f), FVector(5, 6, 7));
	const FMatrix B = FRotationTranslationMatrix(FRotator(-40.f, 15.f, 0.f), FVector(-1, 0, 2));
	MatrixMatchesGlm(*this, "Multiply", A * B, ToGlm(B) * ToGlm(A));
	MatrixMatchesGlm(*this, "Inverse", A.Inverse(), glm::inverse(ToGlm(A)));
	TestEqual("Determinant", A.Determinant(), glm::determinant(ToGlm(A)));

	const glm::vec4 GlmPoint = ToGlm(A) * glm::vec4(1, 2, 3, 1);
	TestTrue("TransformPosition",
		FVector(A.TransformPosition(FVector(1, 2, 3))).Equals(FromGlm(glm::vec3(GlmPoint)), 1.e-4f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGlmInteropQuatTest, "System.Core.Migration.GlmInterop.Quat",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGlmInteropQuatTest::RunTest(const FString& Parameters)
{
	const FVector Axis = FVector(1, 2, 3).GetSafeNormal();
	const FQuat Q(Axis, 0.7f);
	const glm::quat G = glm::angleAxis(0.7f, ToGlm(Axis));

	TestTrue("Axis angle", Q.Equals(FromGlm(G), 1.e-6f));
	TestTrue("RotateVector", Q.RotateVector(FVector(4, 5, 6)).Equals(FromGlm(G * glm::vec3(4, 5, 6)), 1.e-4f));

	const FQuat Other(FVector(0, 0, 1), -1.2f);
	TestTrue("Multiply", (Q * Other).Equals(FromGlm(G * ToGlm(Other)), 1.e-6f));
	TestTrue("Slerp", FQuat::Slerp(Q, Other, 0.3f).Equals(FromGlm(glm::slerp(G, ToGlm(Other), 0.3f)), 1.e-5f));

	const glm::mat4 GlmMatrix = glm::mat4_cast(G);
	TestTrue("Rotation matrix", FQuatRotationMatrix(Q).Equals(FromGlm(GlmMatrix), 1.e-5f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && PLATFORM_DESKTOP
