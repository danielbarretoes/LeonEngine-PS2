#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/HUD.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Tests/EngineTestTypes.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** A local player's controller possessing a default pawn at Location, looking along Rotation. */
	struct FTestPlayer
	{
		APlayerController* Controller = nullptr;
		ADefaultPawn* Pawn = nullptr;

		FTestPlayer(UWorld& World, const FVector& Location, const FRotator& Rotation)
		{
			Pawn = World.SpawnActor<ADefaultPawn>(Location, FRotator::ZeroRotator);
			Controller = World.SpawnActor<APlayerController>();
			Controller->SetPlayer(NewObject<ULocalPlayer>(Controller));
			Controller->Possess(Pawn);
			Controller->SetControlRotation(Rotation);
		}

		void Press(const FKey& Key) const
		{
			(void)Controller->InputKey(Key, IE_Pressed, 1.0f, false);
		}
		void Release(const FKey& Key) const
		{
			(void)Controller->InputKey(Key, IE_Released, 0.0f, false);
		}
	};

	bool HasAxisMapping(const UInputSettings& Settings, const TCHAR* Axis, const FKey& Key, float Scale)
	{
		return Settings.AxisMappings.Contains(FInputAxisKeyMapping(FName(Axis), Key, Scale));
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputSettingsFromBaseInputTest, "System.Engine.Input.SettingsFromBaseInput",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputSettingsFromBaseInputTest::RunTest(const FString& Parameters)
{
	// BaseInput.ini's [/Script/Engine.InputSettings] in UE's struct text: WASD and the arrows move, E / Q go up and
	// down, the mouse turns at 0.3 degrees per pixel, the space bar jumps.
	const UInputSettings& Settings = *GetDefault<UInputSettings>();
	TestTrue("W forward", HasAxisMapping(Settings, TEXT("MoveForward"), EKeys::W, 1.0f));
	TestTrue("Up forward", HasAxisMapping(Settings, TEXT("MoveForward"), EKeys::Up, 1.0f));
	TestTrue("S backward", HasAxisMapping(Settings, TEXT("MoveForward"), EKeys::S, -1.0f));
	TestTrue("Down backward", HasAxisMapping(Settings, TEXT("MoveForward"), EKeys::Down, -1.0f));
	TestTrue("D right", HasAxisMapping(Settings, TEXT("MoveRight"), EKeys::D, 1.0f));
	TestTrue("A left", HasAxisMapping(Settings, TEXT("MoveRight"), EKeys::A, -1.0f));
	TestTrue("Right right", HasAxisMapping(Settings, TEXT("MoveRight"), EKeys::Right, 1.0f));
	TestTrue("Left left", HasAxisMapping(Settings, TEXT("MoveRight"), EKeys::Left, -1.0f));
	TestTrue("E up", HasAxisMapping(Settings, TEXT("MoveUp"), EKeys::E, 1.0f));
	TestTrue("Q down", HasAxisMapping(Settings, TEXT("MoveUp"), EKeys::Q, -1.0f));
	TestTrue("Mouse turns", HasAxisMapping(Settings, TEXT("Turn"), EKeys::MouseX, 1.0f));
	TestTrue("Mouse looks up", HasAxisMapping(Settings, TEXT("LookUp"), EKeys::MouseY, 1.0f));
	TestTrue(
		"Space bar jumps", Settings.ActionMappings.Contains(FInputActionKeyMapping(TEXT("Jump"), EKeys::SpaceBar)));

	const FInputAxisConfigEntry* MouseX = Settings.AxisConfig.FindByPredicate(
		[](const FInputAxisConfigEntry& Entry) { return Entry.AxisKeyName == TEXT("MouseX"); });
	if (TestNotNull("MouseX config", MouseX))
	{
		TestEqual("0.3 degrees per pixel", MouseX->AxisProperties.Sensitivity, 0.3f);
		TestEqual("No dead zone", MouseX->AxisProperties.DeadZone, 0.0f);
	}
	TestTrue("The window keeps the mouse",
		Settings.DefaultViewportMouseCaptureMode == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
	TestTrue("Player input class", UInputSettings::GetDefaultPlayerInputClass() == UPlayerInput::StaticClass());
	TestTrue(
		"Input component class", UInputSettings::GetDefaultInputComponentClass() == UInputComponent::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputKeysAreNamedStructsTest, "System.Engine.Input.KeysAreNamedStructs",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputKeysAreNamedStructsTest::RunTest(const FString& Parameters)
{
	// FKey is a name with UE's text form; EKeys knows each key's kind.
	FKey Key;
	const TCHAR* Text = TEXT("SpaceBar,Scale=1");
	TestTrue("Imports a bare name", Key.ImportTextItem(Text, 0, nullptr, nullptr));
	TestTrue("SpaceBar", Key == EKeys::SpaceBar);
	TestEqual("Stops at the comma", FString(Text), FString(TEXT(",Scale=1")));
	const TCHAR* Quoted = TEXT("\"Gamepad_LeftX\")");
	TestTrue("Imports a quoted name", Key.ImportTextItem(Quoted, 0, nullptr, nullptr));
	TestTrue("Gamepad_LeftX", Key == EKeys::Gamepad_LeftX);
	FString Exported;
	TestTrue("Exports", EKeys::W.ExportTextItem(Exported, FKey(), nullptr, 0, nullptr));
	TestEqual("As its name", Exported, FString(TEXT("W")));

	TestTrue("W is a button", EKeys::W.IsValid() && EKeys::W.IsDigital() && !EKeys::W.IsGamepadKey());
	TestTrue("MouseX is an axis", EKeys::MouseX.IsAxis1D() && EKeys::MouseX.ShouldUpdateAxisWithoutSamples());
	TestTrue("Cross is a gamepad button", EKeys::Gamepad_FaceButton_Bottom.IsGamepadKey());
	TestTrue("Left stick is a gamepad axis", EKeys::Gamepad_LeftX.IsGamepadKey() && EKeys::Gamepad_LeftX.IsAxis1D());
	TestTrue("Shift is a modifier", EKeys::LeftShift.IsModifierKey());
	TestTrue("A mouse button", EKeys::LeftMouseButton.IsMouseButton());
	TestFalse("An unknown key", FKey(TEXT("NoSuchKey")).IsValid());
	TestFalse("No key", EKeys::Invalid.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputKeysDriveAxesAndActionsTest, "System.Engine.Input.KeysDriveAxesAndActions",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputKeysDriveAxesAndActionsTest::RunTest(const FString& Parameters)
{
	// An axis sums its mapped keys times their scales each frame; an action runs once per event, pressed or released;
	// a component higher on the stack takes the keys it binds, and a blocking one takes every key.
	UPlayerInput& Input = *NewObject<UPlayerInput>();
	TestEqual("Starts from the settings", Input.GetKeysForAxis(TEXT("MoveForward")).Num(), 4);
	Input.AddAxisMapping(FInputAxisKeyMapping(TEXT("Strafe"), EKeys::A, -1.0f));
	Input.AddAxisMapping(FInputAxisKeyMapping(TEXT("Strafe"), EKeys::D, 1.0f));
	Input.AddActionMapping(FInputActionKeyMapping(TEXT("Fire"), EKeys::LeftControl));

	UEngineTestInputReceiver& Receiver = *NewObject<UEngineTestInputReceiver>();
	UInputComponent& Component = *NewObject<UInputComponent>();
	Component.BindAxis(TEXT("Strafe"), &Receiver, &UEngineTestInputReceiver::OnAxis);
	Component.BindAction(TEXT("Fire"), IE_Pressed, &Receiver, &UEngineTestInputReceiver::OnPressed);
	Component.BindAction(TEXT("Fire"), IE_Released, &Receiver, &UEngineTestInputReceiver::OnReleased);
	TArray<UInputComponent*> Stack = {&Component};
	const auto Frame = [&Input, &Stack]() { Input.ProcessInputStack(Stack, 1.0f / 60.0f, false); };

	Frame();
	TestEqual("No key: 0", Receiver.LastAxisValue, 0.0f);
	TestEqual("The axis runs every frame", Receiver.AxisCalls, 1);
	(void)Input.InputKey(EKeys::D, IE_Pressed, 1.0f, false);
	Frame();
	TestEqual("D: 1", Component.GetAxisValue(TEXT("Strafe")), 1.0f);
	TestTrue("D just pressed", Input.WasJustPressed(EKeys::D) && Input.IsPressed(EKeys::D));
	(void)Input.InputKey(EKeys::A, IE_Pressed, 1.0f, false);
	Frame();
	TestEqual("D and A: 0", Receiver.LastAxisValue, 0.0f);
	TestFalse("D held, not just pressed", Input.WasJustPressed(EKeys::D));
	(void)Input.InputKey(EKeys::D, IE_Released, 0.0f, false);
	Frame();
	TestEqual("A: -1", Receiver.LastAxisValue, -1.0f);
	TestTrue("D just released", Input.WasJustReleased(EKeys::D) && !Input.IsPressed(EKeys::D));

	(void)Input.InputKey(EKeys::LeftControl, IE_Pressed, 1.0f, false);
	Frame();
	TestEqual("Pressed once", Receiver.Presses, 1);
	Frame();
	TestEqual("Held: no repeat", Receiver.Presses, 1);
	(void)Input.InputKey(EKeys::LeftControl, IE_Released, 0.0f, false);
	Frame();
	TestEqual("Released once", Receiver.Releases, 1);

	// A component on top that binds the axis takes A from the one below; a blocking one takes everything.
	UInputComponent& Top = *NewObject<UInputComponent>();
	Top.BindAxis(TEXT("Strafe"));
	Stack.Add(&Top);
	Frame();
	TestEqual("The top component reads A", Top.GetAxisValue(TEXT("Strafe")), -1.0f);
	TestEqual("The one below does not", Component.GetAxisValue(TEXT("Strafe")), 0.0f);
	UInputComponent& Blocker = *NewObject<UInputComponent>();
	Blocker.bBlockInput = true;
	Stack.Add(&Blocker);
	Frame();
	TestEqual("Blocked", Top.GetAxisValue(TEXT("Strafe")), 0.0f);

	Input.FlushPressedKeys();
	TestFalse("Flushed", Input.IsPressed(EKeys::A));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputMouseTurnsTheControlRotationTest,
	"System.Engine.Input.MouseTurnsTheControlRotation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputMouseTurnsTheControlRotationTest::RunTest(const FString& Parameters)
{
	// A local player's default pawn turns 0.3 degrees per pixel: right turns right, up looks up, and the pawn follows
	// the yaw. Without motion the view stays, and the pitch stops at 89 degrees.
	FScopedTestWorld TestWorld;
	const FTestPlayer Player(*TestWorld, FVector(100.0f, 0.0f, 50.0f), FRotator::ZeroRotator);
	TestNotNull("Player input", Player.Controller->PlayerInput);
	TestNotNull("Pawn input component", Player.Pawn->InputComponent);
	TestNotNull("Camera", Player.Controller->PlayerCameraManager);

	(void)Player.Controller->InputAxis(EKeys::MouseX, 10.0f, 1.0f / 60.0f, 1, false);
	(void)Player.Controller->InputAxis(EKeys::MouseY, 5.0f, 1.0f / 60.0f, 1, false);
	TestWorld->Tick(1.0f / 60.0f);
	TestTrue(
		"3 degrees right, 1.5 up", Player.Controller->GetControlRotation().Equals(FRotator(1.5f, 3.0f, 0.0f), 1.0e-5f));
	TestEqual("The pawn turns with the yaw", Player.Pawn->GetActorRotation().Yaw, 3.0f, 1.0e-5f);
	TestEqual("The pawn keeps its pitch", Player.Pawn->GetActorRotation().Pitch, 0.0f, 1.0e-5f);
	TestTrue("The camera looks along the control rotation",
		Player.Controller->PlayerCameraManager->GetCameraRotation().Equals(FRotator(1.5f, 3.0f, 0.0f), 1.0e-5f));
	TestTrue("The camera is at the pawn",
		Player.Controller->PlayerCameraManager->GetCameraLocation().Equals(FVector(100.0f, 0.0f, 50.0f), 0.0f));

	TestWorld->Tick(1.0f / 60.0f);
	TestTrue("No motion: the view stays",
		Player.Controller->GetControlRotation().Equals(FRotator(1.5f, 3.0f, 0.0f), 1.0e-5f));

	(void)Player.Controller->InputAxis(EKeys::MouseY, 1000.0f, 1.0f / 60.0f, 1, false);
	TestWorld->Tick(1.0f / 60.0f);
	TestEqual("Pitch clamped", Player.Controller->GetControlRotation().Pitch, 89.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputWasdFliesTheDefaultPawnTest, "System.Engine.Input.WasdFliesTheDefaultPawn",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputWasdFliesTheDefaultPawnTest::RunTest(const FString& Parameters)
{
	// The default pawn flies at 800 cm/s: W along the view (pitch included), D to its right, E / Q along world Z, two
	// keys at once along their normalized sum.
	FScopedTestWorld TestWorld;
	const FTestPlayer Player(*TestWorld, FVector::ZeroVector, FRotator::ZeroRotator);
	const auto Step = [&TestWorld, &Player](const FKey& Key, float Seconds)
	{
		const FVector Before = Player.Pawn->GetActorLocation();
		Player.Press(Key);
		TestWorld->Tick(Seconds);
		Player.Release(Key);
		TestWorld->Tick(0.0f);
		return Player.Pawn->GetActorLocation() - Before;
	};
	TestTrue("W: forward", Step(EKeys::W, 0.5f).Equals(FVector(400.0f, 0.0f, 0.0f), 1.0e-3f));
	TestTrue("S: back", Step(EKeys::S, 0.5f).Equals(FVector(-400.0f, 0.0f, 0.0f), 1.0e-3f));
	TestTrue("D: right", Step(EKeys::D, 0.5f).Equals(FVector(0.0f, 400.0f, 0.0f), 1.0e-3f));
	TestTrue("A: left", Step(EKeys::A, 0.5f).Equals(FVector(0.0f, -400.0f, 0.0f), 1.0e-3f));
	TestTrue("E: up", Step(EKeys::E, 0.5f).Equals(FVector(0.0f, 0.0f, 400.0f), 1.0e-3f));
	TestTrue("Q: down", Step(EKeys::Q, 0.5f).Equals(FVector(0.0f, 0.0f, -400.0f), 1.0e-3f));

	Player.Press(EKeys::W);
	Player.Press(EKeys::D);
	FVector Before = Player.Pawn->GetActorLocation();
	TestWorld->Tick(1.0f);
	const float Diagonal = 800.0f / FMath::Sqrt(2.0f);
	TestTrue("W and D: diagonal at 800 cm/s",
		(Player.Pawn->GetActorLocation() - Before).Equals(FVector(Diagonal, Diagonal, 0.0f), 1.0e-2f));
	Player.Release(EKeys::W);
	Player.Release(EKeys::D);
	TestWorld->Tick(0.0f);

	// Looking 30 degrees down and 90 degrees right, W flies along the view.
	Player.Controller->SetControlRotation(FRotator(-30.0f, 90.0f, 0.0f));
	TestTrue("W along the view", Step(EKeys::W, 0.5f).Equals(FRotator(-30.0f, 90.0f, 0.0f).Vector() * 400.0f, 1.0e-2f));
	TestTrue("D stays level", Step(EKeys::D, 0.5f).Equals(FVector(-400.0f, 0.0f, 0.0f), 1.0e-2f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputPlayerControllerOwnsItsHUDAndInputTest,
	"System.Engine.Input.PlayerControllerOwnsItsHUDAndInput",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FInputPlayerControllerOwnsItsHUDAndInputTest::RunTest(const FString& Parameters)
{
	// A local player's controller holds its player input, input component, camera manager and HUD; they go with it.
	FScopedTestWorld TestWorld;
	const FTestPlayer Player(*TestWorld, FVector::ZeroVector, FRotator::ZeroRotator);
	Player.Controller->ClientSetHUD(AHUD::StaticClass());
	TestTrue("Player input inside the controller", Player.Controller->PlayerInput->GetOuter() == Player.Controller);
	TestTrue("HUD owned by the controller",
		Player.Controller->MyHUD != nullptr && Player.Controller->MyHUD->PlayerOwner == Player.Controller);
	TestTrue("Camera for the controller", Player.Controller->PlayerCameraManager->PCOwner == Player.Controller);
	TestNotNull("Controller input component", Player.Controller->InputComponent);

	AHUD* HUD = Player.Controller->MyHUD;
	APlayerCameraManager* Camera = Player.Controller->PlayerCameraManager;
	Player.Controller->Destroy();
	TestTrue("HUD destroyed", HUD->IsPendingKillPending());
	TestTrue("Camera destroyed", Camera->IsPendingKillPending());
	TestNull("The pawn's input goes with the controller", Player.Pawn->InputComponent);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
