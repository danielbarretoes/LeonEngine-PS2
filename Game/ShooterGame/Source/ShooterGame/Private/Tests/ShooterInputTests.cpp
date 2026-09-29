#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "ShooterCharacter.h"
#include "ShooterPlayerController.h"
#include "Tests/ScopedTestWorld.h"
#include "Weapons/ShooterWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS

// The player's input with the project's DefaultInput.ini (Docs/PLANS/ps2-engine.md E4): the PS2 pad's sticks and
// buttons, and the buy menu's own keys, which it takes only while it is open.

namespace
{

	constexpr float FrameTime = 1.0f / 60.0f;

	void TickFrames(UWorld& World, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World.Tick(FrameTime);
		}
	}

	/** A local player's controller possessing a character on a floor, its default weapons drawn. */
	struct FTestShooter
	{
		AShooterPlayerController* Controller = nullptr;
		AShooterCharacter* Character = nullptr;

		explicit FTestShooter(UWorld& World)
		{
			(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
				FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(40.0f, 40.0f, 1.0f)));
			Character = World.SpawnActor<AShooterCharacter>(FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
			Controller = World.SpawnActor<AShooterPlayerController>();
			Controller->SetPlayer(NewObject<ULocalPlayer>(Controller));
			Controller->Possess(Character);
			TickFrames(World, 60);
		}

		void Tap(UWorld& World, const FKey& Key, bool bGamepad) const
		{
			(void)Controller->InputKey(Key, IE_Pressed, 1.0f, bGamepad);
			TickFrames(World, 1);
			(void)Controller->InputKey(Key, IE_Released, 0.0f, bGamepad);
			TickFrames(World, 1);
		}
	};

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameInputPadTest, "ShooterGame.Input.Pad",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameInputPadTest::RunTest(const FString& Parameters)
{
	// The right stick turns at BaseTurnRate degrees a second, the left one walks, and the shoulders draw the slots.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	const FTestShooter Shooter(World);
	AShooterCharacter& Character = *Shooter.Character;
	if (!TestNotNull("A pistol", Character.GetWeaponInSlot(EShooterWeaponSlot::Secondary)))
	{
		return false;
	}

	const float StartYaw = Shooter.Controller->GetControlRotation().Yaw;
	(void)Shooter.Controller->InputAxis(EKeys::Gamepad_RightX, 1.0f, FrameTime, 1, true);
	TickFrames(World, 1);
	TestEqual("A frame at full tilt turns BaseTurnRate / 60", Shooter.Controller->GetControlRotation().Yaw - StartYaw,
		Character.BaseTurnRate * FrameTime, 1.0e-3f);
	(void)Shooter.Controller->InputAxis(EKeys::Gamepad_RightX, 0.0f, FrameTime, 1, true);

	const FVector Start = Character.GetActorLocation();
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		(void)Shooter.Controller->InputAxis(EKeys::Gamepad_LeftY, 1.0f, FrameTime, 1, true);
		TickFrames(World, 1);
	}
	(void)Shooter.Controller->InputAxis(EKeys::Gamepad_LeftY, 0.0f, FrameTime, 1, true);
	TestTrue("The left stick walks forward", Character.GetActorLocation().X - Start.X > 50.0f);

	AShooterWeapon* Rifle = Character.GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("ak47")));
	Shooter.Tap(World, EKeys::Gamepad_LeftShoulder, true);
	TestTrue("L1 draws the pistol", Character.GetWeapon() == Character.GetWeaponInSlot(EShooterWeaponSlot::Secondary));
	Shooter.Tap(World, EKeys::Gamepad_RightShoulder, true);
	TestTrue("R1 draws the rifle", Rifle != nullptr && Character.GetWeapon() == Rifle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameInputBuyMenuTest, "ShooterGame.Input.BuyMenuTakesItsKeys",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameInputBuyMenuTest::RunTest(const FString& Parameters)
{
	// Closed, the buy menu leaves the number keys to the weapon slots. Open (Start), the D-pad moves its highlight,
	// Cross opens the highlighted category and buys an item instead of jumping, and Circle goes back to the first page
	// and then closes it instead of crouching.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	const FTestShooter Shooter(World);
	AShooterCharacter& Character = *Shooter.Character;
	AShooterPlayerController& Controller = *Shooter.Controller;
	AShooterWeapon* Rifle = Character.GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("ak47")));
	Character.SelectSlot(EShooterWeaponSlot::Secondary);
	TickFrames(World, 2);
	Shooter.Tap(World, EKeys::One, false);
	TestTrue("1 draws the rifle with the menu closed", Rifle != nullptr && Character.GetWeapon() == Rifle);

	Shooter.Tap(World, EKeys::Gamepad_Special_Right, true);
	TestTrue("Start opens the menu", Controller.IsBuyMenuOpen());
	Shooter.Tap(World, EKeys::Gamepad_DPad_Down, true);
	Shooter.Tap(World, EKeys::Gamepad_DPad_Down, true);
	TestEqual("Down twice: the third item", Controller.GetBuyMenuSelection(), 2);
	Shooter.Tap(World, EKeys::Gamepad_DPad_Up, true);
	Shooter.Tap(World, EKeys::Gamepad_DPad_Up, true);
	Shooter.Tap(World, EKeys::Gamepad_DPad_Up, true);
	TestEqual("Up past the first wraps to the last", Controller.GetBuyMenuSelection(),
		AShooterPlayerController::GetNumBuyMenuCategories() - 1);
	// The last category (the equipment): Cross opens it; then its first item. Without a game mode a buy is refused,
	// but the attempt reaches the menu (its message).
	Shooter.Tap(World, EKeys::Gamepad_FaceButton_Bottom, true);
	TestEqual("Cross opens the equipment", Controller.GetBuyMenuCategory(),
		AShooterPlayerController::GetNumBuyMenuCategories() - 1);
	TestTrue("and does not jump", Character.IsMovingOnGround());
	Shooter.Tap(World, EKeys::Gamepad_FaceButton_Bottom, true);
	TestFalse("Cross tries to buy the vest", Controller.GetLastBuyMessage().IsEmpty());
	TestEqual("then the first page", Controller.GetBuyMenuCategory(), static_cast<int32>(INDEX_NONE));
	Controller.SetBuyMenuOpen(false);
	Character.SelectSlot(EShooterWeaponSlot::Secondary);
	TickFrames(World, 2);
	Controller.SetBuyMenuOpen(true);
	Shooter.Tap(World, EKeys::One, false);
	TestEqual("1 opens the pistols", Controller.GetBuyMenuCategory(), 0);
	TestTrue("instead of drawing the rifle",
		Character.GetWeapon() == Character.GetWeaponInSlot(EShooterWeaponSlot::Secondary));
	Shooter.Tap(World, EKeys::Gamepad_FaceButton_Right, true);
	TestTrue("Circle goes back to the first page",
		Controller.IsBuyMenuOpen() && Controller.GetBuyMenuCategory() == INDEX_NONE);
	Shooter.Tap(World, EKeys::Gamepad_FaceButton_Right, true);
	TestFalse("and then closes the menu", Controller.IsBuyMenuOpen());
	TestFalse("and does not crouch", Character.bIsCrouched);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
