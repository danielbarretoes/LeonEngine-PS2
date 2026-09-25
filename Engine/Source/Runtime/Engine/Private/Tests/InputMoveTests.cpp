#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Input.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMoveMoveAxes2DAnyDetectsNonzeroTest,
	"System.Engine.InputMove.MoveAxes2DAnyDetectsNonzero",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMoveMoveAxes2DAnyDetectsNonzeroTest::RunTest(const FString& Parameters)
{
	// Any() is false for zero axes and true when either axis is nonzero.
	const FMoveAxes2D Zero{};
	TestFalse("Zero axes", Zero.Any());
	TestTrue("Strafe axis", FMoveAxes2D{1.0f, 0.0f}.Any());
	TestTrue("Forward axis", FMoveAxes2D{0.0f, -1.0f}.Any());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMoveYawRelativeMoveXzReturnsZeroWithoutAxesTest,
	"System.Engine.InputMove.YawRelativeMoveXzReturnsZeroWithoutAxes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMoveYawRelativeMoveXzReturnsZeroWithoutAxesTest::RunTest(const FString& Parameters)
{
	// No input gives no move, whatever the yaw.
	const FVector Move = YawRelativeMoveXz(45.0f, FMoveAxes2D{});
	TestEqual("Move X", Move.X, 0.0f, 1.0e-6f);
	TestEqual("Move Y", Move.Y, 0.0f, 1.0e-6f);
	TestEqual("Move Z", Move.Z, 0.0f, 1.0e-6f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMoveYawRelativeMoveXzForwardAtYawZeroTest,
	"System.Engine.InputMove.YawRelativeMoveXzForwardAtYawZero",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMoveYawRelativeMoveXzForwardAtYawZeroTest::RunTest(const FString& Parameters)
{
	// Forward input at yaw 0 is a unit move along -X.
	const FVector Move = YawRelativeMoveXz(0.0f, FMoveAxes2D{0.0f, 1.0f});
	TestEqual("Move length", Move.Size(), 1.0f, 1.0e-4f);
	TestEqual("Move X", Move.X, -1.0f, 1.0e-4f);
	TestEqual("Move Y", Move.Y, 0.0f, 1.0e-4f);
	TestEqual("Move Z", Move.Z, 0.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMoveCameraRelativeMoveXzMatchesCameraYawTest,
	"System.Engine.InputMove.CameraRelativeMoveXzMatchesCameraYaw",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMoveCameraRelativeMoveXzMatchesCameraYawTest::RunTest(const FString& Parameters)
{
	// The camera-relative move equals the yaw-relative move for the camera's yaw.
	UCameraComponent Cam;
	Cam.SetYawPitch(90.0f, 0.0f);
	const FVector A = CameraRelativeMoveXz(Cam, FMoveAxes2D{0.0f, 1.0f});
	const FVector B = YawRelativeMoveXz(90.0f, FMoveAxes2D{0.0f, 1.0f});
	TestEqual("Move X", A.X, B.X, 1.0e-5f);
	TestEqual("Move Z", A.Z, B.Z, 1.0e-5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
