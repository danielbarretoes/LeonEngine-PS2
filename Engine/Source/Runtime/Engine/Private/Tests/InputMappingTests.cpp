#include "CoreMinimal.h"
#include "GameFramework/InputActions.h"
#include "GameFramework/InputMapping.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMappingMakeDefaultBindsMoveAndJumpTest,
	"System.Engine.InputMapping.MakeDefaultBindsMoveAndJump",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMappingMakeDefaultBindsMoveAndJumpTest::RunTest(const FString& Parameters)
{
	// The default context binds the three move axes and the jump action (Space bar first).
	const UInputMappingContext Ctx = UInputMappingContext::MakeDefault();

	TestTrue("MoveForward axis bound", Ctx.GetAxes().Contains(FName(Leon::InputActions::MoveForward)));
	TestTrue("MoveRight axis bound", Ctx.GetAxes().Contains(FName(Leon::InputActions::MoveRight)));
	TestTrue("MoveUp axis bound", Ctx.GetAxes().Contains(FName(Leon::InputActions::MoveUp)));
	TestTrue("Jump action bound", Ctx.GetActions().Contains(FName(Leon::InputActions::Jump)));

	const TArray<int32>* JumpKeys = Ctx.GetActions().Find(FName(Leon::InputActions::Jump));
	if (!TestNotNull("Jump keys", JumpKeys))
	{
		return false;
	}
	if (!TestFalse("Jump keys empty", JumpKeys->Num() == 0))
	{
		return false;
	}
	TestEqual("First jump key", (*JumpKeys)[0], ToKeyCode(EKeys::SpaceBar));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMappingBindAxisKeyAndBindActionKeyTest,
	"System.Engine.InputMapping.BindAxisKeyAndBindActionKey",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMappingBindAxisKeyAndBindActionKeyTest::RunTest(const FString& Parameters)
{
	// Axis and action binds accumulate per name; an empty name or a zero key is ignored.
	UInputMappingContext Ctx;
	Ctx.BindAxisKey(FName("Strafe"), EKeys::A, -1.0f);
	Ctx.BindAxisKey(FName("Strafe"), EKeys::D, 1.0f);
	Ctx.BindActionKey(FName("Fire"), EKeys::LeftControl);
	Ctx.BindAxisKey(FName(""), EKeys::W, 1.0f); // ignored
	Ctx.BindActionKey(FName("Fire"), 0); // ignored

	const TArray<FInputAxisKeyMapping>* StrafeKeys = Ctx.GetAxes().Find(FName("Strafe"));
	if (!TestNotNull("Strafe axis", StrafeKeys))
	{
		return false;
	}
	TestEqual("Strafe keys", StrafeKeys->Num(), 2);

	const TArray<int32>* FireKeys = Ctx.GetActions().Find(FName("Fire"));
	if (!TestNotNull("Fire action", FireKeys))
	{
		return false;
	}
	if (!TestEqual("Fire keys", FireKeys->Num(), 1))
	{
		return false;
	}
	TestEqual("Fire key", (*FireKeys)[0], ToKeyCode(EKeys::LeftControl));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMappingPlayerInputClearContextsEmptiesMapsAfterUpdatePathTest,
	"System.Engine.InputMapping.PlayerInputClearContextsEmptiesMapsAfterUpdatePath",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMappingPlayerInputClearContextsEmptiesMapsAfterUpdatePathTest::RunTest(const FString& Parameters)
{
	// After ClearContexts no axis, action or move input is reported.
	UPlayerInput Input;
	Input.AddMappingContext(UInputMappingContext::MakeDefault());
	Input.ClearContexts();
	TestEqual("MoveForward axis", Input.GetAxisValue(FName(Leon::InputActions::MoveForward)), 0.0f, 1.0e-6f);
	TestFalse("Jump pressed", Input.IsActionPressed(FName(Leon::InputActions::Jump)));
	TestTrue("Move input", Input.GetMoveInput().IsZero());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
