#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/DualShockForceFeedback.h"
#include "GenericPlatform/IInputInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A pad that keeps what its motors are sent. */
	class FForceFeedbackTestPad final : public IInputInterface
	{
	public:
		int32 GetNumControllers() const override
		{
			return MaxControllers;
		}
		bool IsGamepadConnected(int32) const override
		{
			return true;
		}
		bool IsGamepadKeyDown(int32, const FKey&) const override
		{
			return false;
		}
		float GetGamepadAnalog(int32, const FKey&) const override
		{
			return 0.0f;
		}
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForceFeedbackPlayerControllerTest, "System.Engine.ForceFeedback.PlayerController",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FForceFeedbackPlayerControllerTest::RunTest(const FString& Parameters)
{
	// A local player's controller plays force feedback effects (ps2-shipping N24; UE's API): their curves add up per
	// channel (the stronger), go to the player's controller id each tick, and stop at their end; a tag replaces, a
	// looping effect goes on until stopped, and a player who turned vibration off feels nothing.
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
	FString Error;
	(void)Engine->Browse(*Engine->GameInstance->GetWorldContext(), FURL(nullptr, *StarterMap, TRAVEL_Absolute), Error);
	APlayerController* Controller = Engine->GameInstance->GetFirstGamePlayer()->PlayerController;
	if (Controller == nullptr)
	{
		AddError(TEXT("The map did not open with a player"));
		GEngine = SavedEngine;
		Engine->PreExit();
		return false;
	}
	FForceFeedbackTestPad Pad;
	Engine->GameViewport->SetInputInterface(&Pad);

	TStrongObjectPtr<UForceFeedbackEffect> Fire(NewObject<UForceFeedbackEffect>());
	Fire->Duration = 0.2f;
	FForceFeedbackChannelDetails Small;
	Small.bAffectsLeftLarge = false;
	Small.bAffectsRightLarge = false;
	Small.StartIntensity = 1.0f;
	Small.EndIntensity = 0.0f;
	FForceFeedbackChannelDetails Large;
	Large.bAffectsLeftSmall = false;
	Large.bAffectsRightSmall = false;
	Large.StartIntensity = 0.5f;
	Large.EndIntensity = 0.5f;
	Fire->ChannelDetails = {Small, Large};

	Controller->ClientPlayForceFeedback(Fire.Get());
	Controller->ProcessForceFeedbackAndHaptics(0.05f, false);
	const FForceFeedbackValues& Sent = Pad.GetForceFeedbackValues(0);
	TestEqual("The small channels a quarter of the way", Sent.LeftSmall, 0.75f, 1.0e-5f);
	TestEqual("... both", Sent.RightSmall, 0.75f, 1.0e-5f);
	TestEqual("The large ones", Sent.LeftLarge, 0.5f, 1.0e-5f);
	TestEqual("Nothing to the second controller", Pad.GetForceFeedbackValues(1).LeftLarge, 0.0f);
	const FDualShockMotors Motors = FDualShockForceFeedback::ToMotors(Sent);
	TestTrue("On the DualShock: the small motor on, the large at half",
		Motors.Small == 1 && Motors.Large > 0x40 && Motors.Large < 0xff);

	Controller->ProcessForceFeedbackAndHaptics(0.2f, false);
	TestEqual("Over after its duration", Controller->ActiveForceFeedbackEffects.Num(), 0);
	TestEqual("Still", Pad.GetForceFeedbackValues(0).LeftLarge, 0.0f);

	FForceFeedbackParameters Tagged;
	Tagged.Tag = TEXT("Weapon");
	Controller->ClientPlayForceFeedback(Fire.Get(), Tagged);
	Controller->ClientPlayForceFeedback(Fire.Get(), Tagged);
	TestEqual("A tag replaces", Controller->ActiveForceFeedbackEffects.Num(), 1);
	FForceFeedbackParameters Looping;
	Looping.bLooping = true;
	Controller->ClientPlayForceFeedback(Fire.Get(), Looping);
	Controller->ProcessForceFeedbackAndHaptics(1.0f, false);
	TestEqual("A looping effect goes on", Controller->ActiveForceFeedbackEffects.Num(), 1);
	TestTrue("... and vibrates", Pad.GetForceFeedbackValues(0).LeftLarge > 0.0f);
	Controller->bForceFeedbackEnabled = false;
	Controller->ProcessForceFeedbackAndHaptics(0.05f, false);
	TestEqual("Vibration off", Pad.GetForceFeedbackValues(0).LeftLarge, 0.0f);
	Controller->bForceFeedbackEnabled = true;
	Controller->ClientStopForceFeedback(nullptr, NAME_None);
	Controller->ProcessForceFeedbackAndHaptics(0.05f, false);
	TestEqual("Stopped", Controller->ActiveForceFeedbackEffects.Num(), 0);
	TestEqual("... still", Pad.GetForceFeedbackValues(0).LeftSmall, 0.0f);

	Engine->GameViewport->SetInputInterface(nullptr);
	GEngine = SavedEngine;
	Engine->PreExit();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
