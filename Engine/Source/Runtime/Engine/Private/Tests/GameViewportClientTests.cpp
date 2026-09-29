#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GenericPlatform/GenericWindow.h"
#include "GenericPlatform/IInputInterface.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** A window without keys or a mouse (the PS2's). */
	class FTestWindow final : public FGenericWindow
	{
	public:
		bool Create(int32 InWidth, int32 InHeight, const TCHAR* /*Title*/) override
		{
			WindowWidth = InWidth;
			WindowHeight = InHeight;
			FramebufferWidth = InWidth;
			FramebufferHeight = InHeight;
			return true;
		}
		void Destroy() override
		{
		}
		bool IsFocused() const override
		{
			return true;
		}
		bool ShouldClose() const override
		{
			return false;
		}
		void PollEvents() override
		{
		}
		void SwapBuffers() override
		{
		}
	};

	/** Two pads whose buttons, sticks and pressures the test sets. */
	class FTestPad final : public IInputInterface
	{
	public:
		bool bConnected[MaxControllers] = {true, true};
		TSet<FKey> Down[MaxControllers];
		TMap<FKey, float> Axes[MaxControllers];

		int32 GetNumControllers() const override
		{
			return MaxControllers;
		}
		bool IsGamepadConnected(int32 ControllerId) const override
		{
			return bConnected[ControllerId];
		}
		bool IsGamepadKeyDown(int32 ControllerId, const FKey& Key) const override
		{
			return Down[ControllerId].Contains(Key);
		}
		float GetGamepadAnalog(int32 ControllerId, const FKey& Axis) const override
		{
			const float* Value = Axes[ControllerId].Find(Axis);
			return Value != nullptr ? *Value : 0.0f;
		}
	};

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameViewportClientIgnoreInputTest, "System.Engine.Viewport.IgnoreInput",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameViewportClientIgnoreInputTest::RunTest(const FString& Parameters)
{
	// An unattended run (a -Screenshot capture) ignores the OS input: the viewport client drops the mouse and key
	// events, so the view stays where the map put it. With the input back, the same mouse sample turns the view.
	const FString StarterMap = TEXT("/Engine/Maps/Template_Default");
	if (!FPackageName::DoesPackageExist(StarterMap))
	{
		AddError(TEXT("Template_Default.lmap not found"));
		return false;
	}
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	UEngine* const SavedEngine = GEngine;
	GEngine = Engine.Get();
	Engine->Init(nullptr);
	FWorldContext& Context = *Engine->GameInstance->GetWorldContext();
	FString Error;
	(void)Engine->Browse(Context, FURL(nullptr, *StarterMap, TRAVEL_Absolute), Error);
	UWorld* World = Engine->GetGameWorld();
	UGameViewportClient& Viewport = *Engine->GameViewport;
	APlayerController* Controller = Engine->GameInstance->GetFirstGamePlayer()->PlayerController;
	if (World == nullptr || Controller == nullptr)
	{
		AddError(TEXT("The map did not open with a player"));
		GEngine = SavedEngine;
		Engine->PreExit();
		return false;
	}
	const FRotator Start = Controller->GetControlRotation();

	Viewport.SetIgnoreInput(true);
	TestTrue("Ignoring", Viewport.IgnoreInput());
	TestFalse("The mouse is dropped", Viewport.InputAxis(nullptr, 0, EKeys::MouseX, 100.0f, 1.0f / 60.0f));
	TestFalse("A key is dropped", Viewport.InputKey(nullptr, 0, EKeys::W, IE_Pressed));
	Viewport.ProcessInput(1.0f / 60.0f);
	World->Tick(1.0f / 60.0f);
	TestTrue("The view stays", Controller->GetControlRotation().Equals(Start, 0.0f));

	Viewport.SetIgnoreInput(false);
	TestTrue("The mouse reaches the player", Viewport.InputAxis(nullptr, 0, EKeys::MouseX, 100.0f, 1.0f / 60.0f));
	World->Tick(1.0f / 60.0f);
	TestFalse("The view turns", Controller->GetControlRotation().Equals(Start, 1.0e-3f));

	GEngine = SavedEngine;
	Engine->PreExit();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameViewportClientGamepadTest, "System.Engine.Viewport.Gamepad",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameViewportClientGamepadTest::RunTest(const FString& Parameters)
{
	// The application's pad reaches the player: a button's changes as pressed and released gamepad keys, a stick as a
	// sample every frame (0 once released, so the axis stops), and a pad that goes away releases its buttons.
	const FString StarterMap = TEXT("/Engine/Maps/Template_Default");
	if (!FPackageName::DoesPackageExist(StarterMap))
	{
		AddError(TEXT("Template_Default.lmap not found"));
		return false;
	}
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	UEngine* const SavedEngine = GEngine;
	GEngine = Engine.Get();
	Engine->Init(nullptr);
	FWorldContext& Context = *Engine->GameInstance->GetWorldContext();
	FString Error;
	(void)Engine->Browse(Context, FURL(nullptr, *StarterMap, TRAVEL_Absolute), Error);
	UWorld* World = Engine->GetGameWorld();
	UGameViewportClient& Viewport = *Engine->GameViewport;
	APlayerController* Controller = Engine->GameInstance->GetFirstGamePlayer()->PlayerController;
	if (World == nullptr || Controller == nullptr || Controller->PlayerInput == nullptr)
	{
		AddError(TEXT("The map did not open with a player"));
		GEngine = SavedEngine;
		Engine->PreExit();
		return false;
	}
	FTestWindow Window;
	(void)Window.Create(640, 448, TEXT("Test"));
	FTestPad Pad;
	Viewport.SetViewportWindow(&Window);
	Viewport.SetInputInterface(&Pad);
	const UPlayerInput& Input = *Controller->PlayerInput;
	const auto Frame = [&Viewport, World]()
	{
		Viewport.ProcessInput(1.0f / 60.0f);
		World->Tick(1.0f / 60.0f);
	};

	Pad.Down[0].Add(EKeys::Gamepad_FaceButton_Bottom);
	Pad.Axes[0].Add(EKeys::Gamepad_RightX, 0.5f);
	Pad.Axes[0].Add(EKeys::Gamepad_FaceButton_BottomAxis, 0.25f);
	Frame();
	TestTrue("Cross is down", Input.IsPressed(EKeys::Gamepad_FaceButton_Bottom));
	TestEqual("The right stick", Input.GetRawKeyValue(EKeys::Gamepad_RightX), 0.5f);
	TestEqual("Cross's pressure", Input.GetRawKeyValue(EKeys::Gamepad_FaceButton_BottomAxis), 0.25f);
	Frame();
	TestTrue("Still down", Input.IsPressed(EKeys::Gamepad_FaceButton_Bottom));
	TestEqual("A held stick keeps its value", Input.GetRawKeyValue(EKeys::Gamepad_RightX), 0.5f);
	TestEqual("A held pressure keeps its value", Input.GetRawKeyValue(EKeys::Gamepad_FaceButton_BottomAxis), 0.25f);

	Pad.Axes[0].Reset();
	Pad.Down[0].Reset();
	Frame();
	TestFalse("Cross released", Input.IsPressed(EKeys::Gamepad_FaceButton_Bottom));
	TestEqual("A released stick reads 0", Input.GetRawKeyValue(EKeys::Gamepad_RightX), 0.0f);
	TestEqual("A released button's pressure reads 0", Input.GetRawKeyValue(EKeys::Gamepad_FaceButton_BottomAxis), 0.0f);

	// The second pad is controller 1: no local player uses it, so it moves no one (ps2-shipping N24).
	Pad.Down[1].Add(EKeys::Gamepad_FaceButton_Bottom);
	Pad.Axes[1].Add(EKeys::Gamepad_RightX, 1.0f);
	Frame();
	TestFalse("The second pad does not press Cross", Input.IsPressed(EKeys::Gamepad_FaceButton_Bottom));
	TestEqual("Nor turn", Input.GetRawKeyValue(EKeys::Gamepad_RightX), 0.0f);
	TestTrue("No player for it", Viewport.FindLocalPlayerFromControllerId(1) == nullptr);
	TestTrue("Controller 0 is the player's", Viewport.FindLocalPlayerFromControllerId(0) != nullptr);
	Pad.Down[1].Reset();
	Pad.Axes[1].Reset();

	Pad.Down[0].Add(EKeys::Gamepad_RightTrigger);
	Frame();
	TestTrue("R2 is down", Input.IsPressed(EKeys::Gamepad_RightTrigger));
	Pad.bConnected[0] = false;
	Frame();
	TestFalse("A pad that goes away lets go", Input.IsPressed(EKeys::Gamepad_RightTrigger));

	Viewport.SetInputInterface(nullptr);
	Viewport.SetViewportWindow(nullptr);
	GEngine = SavedEngine;
	Engine->PreExit();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
