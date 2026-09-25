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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMoveYawRelativeMoveReturnsZeroWithoutAxesTest,
	"System.Engine.InputMove.YawRelativeMoveReturnsZeroWithoutAxes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMoveYawRelativeMoveReturnsZeroWithoutAxesTest::RunTest(const FString& Parameters)
{
	// No input gives no move, whatever the yaw.
	const FVector Move = YawRelativeMove(45.0f, FMoveAxes2D{});
	TestEqual("Move X", Move.X, 0.0f, 1.0e-6f);
	TestEqual("Move Y", Move.Y, 0.0f, 1.0e-6f);
	TestEqual("Move Z", Move.Z, 0.0f, 1.0e-6f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMoveYawRelativeMoveForwardAtYawZeroTest,
	"System.Engine.InputMove.YawRelativeMoveForwardAtYawZero",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMoveYawRelativeMoveForwardAtYawZeroTest::RunTest(const FString& Parameters)
{
	// Forward input at view yaw 0 is a unit move along +X; strafing right at yaw 0 moves along +Y.
	const FVector Move = YawRelativeMove(0.0f, FMoveAxes2D{0.0f, 1.0f});
	TestEqual("Move length", Move.Size(), 1.0f, 1.0e-4f);
	TestEqual("Move X", Move.X, 1.0f, 1.0e-4f);
	TestEqual("Move Y", Move.Y, 0.0f, 1.0e-4f);
	TestEqual("Move Z", Move.Z, 0.0f, 1.0e-4f);
	const FVector Strafe = YawRelativeMove(0.0f, FMoveAxes2D{1.0f, 0.0f});
	TestTrue("Strafe right is +Y", Strafe.Equals(FVector(0.0f, 1.0f, 0.0f), 1.0e-5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMoveCameraRelativeMoveMatchesCameraYawTest,
	"System.Engine.InputMove.CameraRelativeMoveMatchesCameraYaw",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMoveCameraRelativeMoveMatchesCameraYawTest::RunTest(const FString& Parameters)
{
	// The camera-relative move equals the yaw-relative move for the camera's view yaw, and follows its forward on the
	// ground even when the camera looks down.
	UCameraComponent Cam;
	Cam.SetYawPitch(90.0f, -40.0f);
	const FVector A = CameraRelativeMove(Cam, FMoveAxes2D{0.0f, 1.0f});
	const FVector B = YawRelativeMove(90.0f, FMoveAxes2D{0.0f, 1.0f});
	TestEqual("Move X", A.X, B.X, 1.0e-5f);
	TestEqual("Move Y", A.Y, B.Y, 1.0e-5f);
	const FVector Forward = Cam.ForwardVector();
	TestTrue("Along the view on the ground", A.Equals(FVector(Forward.X, Forward.Y, 0.0f).GetSafeNormal(), 1.0e-5f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
