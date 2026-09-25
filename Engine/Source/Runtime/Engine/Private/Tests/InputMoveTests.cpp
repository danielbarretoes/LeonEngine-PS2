#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Input.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMoveEitherAxisMovesTest, "System.Engine.InputMove.EitherAxisMoves",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMoveEitherAxisMovesTest::RunTest(const FString& Parameters)
{
	// Either move axis alone gives a unit move: backward input (X < 0) goes along -X at yaw 0, left input (Y < 0)
	// along -Y.
	const FRotator View = FRotator::ZeroRotator;
	TestTrue("Backward", YawRelativeMove(View, FVector2D(-1.0f, 0.0f)).Equals(FVector(-1.0f, 0.0f, 0.0f), 1.0e-5f));
	TestTrue("Left", YawRelativeMove(View, FVector2D(0.0f, -1.0f)).Equals(FVector(0.0f, -1.0f, 0.0f), 1.0e-5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMoveYawRelativeMoveReturnsZeroWithoutAxesTest,
	"System.Engine.InputMove.YawRelativeMoveReturnsZeroWithoutAxes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMoveYawRelativeMoveReturnsZeroWithoutAxesTest::RunTest(const FString& Parameters)
{
	// No input gives no move, whatever the yaw.
	const FVector Move = YawRelativeMove(FRotator(0.0f, 45.0f, 0.0f), FVector2D::ZeroVector);
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
	// Forward input (X) at view yaw 0 is a unit move along +X; right input (Y) at yaw 0 moves along +Y.
	const FVector Move = YawRelativeMove(FRotator::ZeroRotator, FVector2D(1.0f, 0.0f));
	TestEqual("Move length", Move.Size(), 1.0f, 1.0e-4f);
	TestEqual("Move X", Move.X, 1.0f, 1.0e-4f);
	TestEqual("Move Y", Move.Y, 0.0f, 1.0e-4f);
	TestEqual("Move Z", Move.Z, 0.0f, 1.0e-4f);
	const FVector Strafe = YawRelativeMove(FRotator::ZeroRotator, FVector2D(0.0f, 1.0f));
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
	Cam.SetViewRotation(FRotator(-40.0f, 90.0f, 0.0f));
	const FVector A = CameraRelativeMove(Cam, FVector2D(1.0f, 0.0f));
	const FVector B = YawRelativeMove(FRotator(0.0f, 90.0f, 0.0f), FVector2D(1.0f, 0.0f));
	TestEqual("Move X", A.X, B.X, 1.0e-5f);
	TestEqual("Move Y", A.Y, B.Y, 1.0e-5f);
	const FVector Forward = Cam.ForwardVector();
	TestTrue("Along the view on the ground", A.Equals(FVector(Forward.X, Forward.Y, 0.0f).GetSafeNormal(), 1.0e-5f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
