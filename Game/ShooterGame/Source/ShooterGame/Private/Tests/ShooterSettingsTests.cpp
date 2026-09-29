#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "SaveGameSystem.h"
#include "ShooterPersistentUser.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterSettingsRoundTripTest, "ShooterGame.Settings.RoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterSettingsRoundTripTest::RunTest(const FString& Parameters)
{
	// The player's options (ps2-shipping N24) go to the "Settings" slot and come back; without a save they are the
	// defaults. The slot is a folder of the test's here (Saved/SaveGames/ in the game, the memory card on the PS2).
	FGenericSaveGameSystem Files;
	const FString Dir = FPaths::ProjectIntermediateDir() + TEXT("Tests/ShooterSettings/");
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	Files.SetSaveDirectory(Dir);
	IPlatformFeaturesModule::Get().SetSaveGameSystemOverride(&Files);

	TStrongObjectPtr<UShooterPersistentUser> Defaults(UShooterPersistentUser::LoadPersistentUser(0));
	TestTrue("The defaults without a save",
		Defaults.IsValid() && Defaults->AimSensitivity == 1.0f && !Defaults->bInvertedYAxis &&
			Defaults->SoundVolume == 1.0f);
	Defaults->AimSensitivity = 1.75f;
	Defaults->bInvertedYAxis = true;
	Defaults->SoundVolume = 0.3f;
	Defaults->CrosshairColor = FLinearColor(1.0f, 0.0f, 1.0f);
	TestTrue("Saved", Defaults->SaveToSlot(0));
	TestTrue("In the Settings slot", IFileManager::Get().FileExists(*(Dir + TEXT("Settings.sav"))));
	TStrongObjectPtr<UShooterPersistentUser> Loaded(UShooterPersistentUser::LoadPersistentUser(0));
	TestTrue("The sensitivity", Loaded.IsValid() && Loaded->AimSensitivity == 1.75f);
	TestTrue("The inverted Y axis", Loaded.IsValid() && Loaded->bInvertedYAxis);
	TestTrue("The volume", Loaded.IsValid() && Loaded->SoundVolume == 0.3f);
	TestTrue("The crosshair", Loaded.IsValid() && Loaded->CrosshairColor.Equals(FLinearColor(1.0f, 0.0f, 1.0f)));

	IPlatformFeaturesModule::Get().SetSaveGameSystemOverride(nullptr);
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
