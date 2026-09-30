#include "CanvasTypes.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterHUD.h"
#include "ShooterPersistentUser.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterWeapon_Instant.h"

#if WITH_DEV_AUTOMATION_TESTS

// The player's input with the project's DefaultInput.ini (Docs/PLANS/ps2-engine.md E4): the PS2 pad's sticks and
// buttons, and the buy menu's own keys, which it takes only while it is open; the crouch toggle, and dropping and
// picking up the weapons and the bomb (ps2-polish P4).

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

		/** The player's HUD (made on demand), painted into a 640 x 448 canvas. */
		AShooterHUD* PaintHUD() const
		{
			if (Controller->MyHUD == nullptr)
			{
				Controller->ClientSetHUD(AShooterHUD::StaticClass());
			}
			AShooterHUD* HUD = Cast<AShooterHUD>(Controller->MyHUD);
			if (HUD != nullptr)
			{
				FCanvas Canvas(640, 448);
				HUD->Paint(Canvas);
			}
			return HUD;
		}

		/** Moves the pawn over Location (its height kept: on the floor). */
		void StandOn(const FVector& Location) const
		{
			const FVector Here = Character->GetActorLocation();
			(void)Character->SetActorLocation(FVector(Location.X, Location.Y, Here.Z));
		}
	};

	/** Seconds as whole 60 Hz frames, and one more. */
	int32 FramesFor(float Seconds)
	{
		return FMath::CeilToInt(Seconds / FrameTime) + 1;
	}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameInputCrouchToggleTest, "ShooterGame.Input.CrouchToggleAndHold",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameInputCrouchToggleTest::RunTest(const FString& Parameters)
{
	// ps2-polish P4: by default a press of the crouch key (Left Ctrl, Circle) crouches and the next stands up; with the
	// option off (SetToggleCrouch 0) the key is held. A new round stands the pawn up either way.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	const FTestShooter Shooter(World);
	AShooterCharacter& Character = *Shooter.Character;
	AShooterPlayerController& Controller = *Shooter.Controller;
	TestTrue("A toggle by default", Controller.IsCrouchToggle() && GetDefault<UShooterPersistentUser>()->bToggleCrouch);

	Shooter.Tap(World, EKeys::LeftControl, false);
	TickFrames(World, 2);
	TestTrue("A tap crouches", Character.bIsCrouched);
	TickFrames(World, 30);
	TestTrue("and it stays crouched", Character.bIsCrouched);
	Shooter.Tap(World, EKeys::LeftControl, false);
	TickFrames(World, 2);
	TestFalse("The next tap stands up", Character.bIsCrouched);
	Shooter.Tap(World, EKeys::Gamepad_FaceButton_Right, true);
	TickFrames(World, 2);
	TestTrue("Circle toggles too", Character.bIsCrouched);
	Character.ResetForNewRound(FVector(0.0f, 0.0f, 100.0f), 0.0f);
	TickFrames(World, 2);
	TestFalse("A new round stands it up", Character.bIsCrouched || Character.GetCharacterMovement().bWantsToCrouch);

	// Held: down while pressed, up on release.
	Controller.SetToggleCrouch(0);
	TestFalse("The option off", Controller.IsCrouchToggle());
	(void)Controller.InputKey(EKeys::LeftControl, IE_Pressed, 1.0f, false);
	TickFrames(World, 30);
	TestTrue("Held: crouched", Character.bIsCrouched);
	(void)Controller.InputKey(EKeys::LeftControl, IE_Released, 0.0f, false);
	TickFrames(World, 2);
	TestFalse("Released: standing", Character.bIsCrouched);
	Controller.SetToggleCrouch(1);
	TestTrue("The option on again", Controller.IsCrouchToggle());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameInputDropAndPickUpWeaponTest, "ShooterGame.Input.DropAndPickUpWeapon",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameInputDropAndPickUpWeaponTest::RunTest(const FString& Parameters)
{
	// G drops the rifle in hand (and draws the pistol); walking over it with the primary slot free takes it back, with
	// its rounds and its silencer, not drawn, and the HUD says "Picked up m4a1" for PickupNoticeDuration. With the slot
	// taken the pawn walks over a weapon and leaves it.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	(void)World.SetGameMode(AShooterGameMode::StaticClass());
	const FTestShooter Shooter(World);
	AShooterCharacter& Character = *Shooter.Character;
	AShooterPlayerController& Controller = *Shooter.Controller;
	AShooterWeapon_Instant* Rifle =
		Cast<AShooterWeapon_Instant>(Character.GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("m4a1"))));
	if (!TestNotNull("An M4A1", Rifle))
	{
		return false;
	}
	(void)Rifle->GiveAmmo(Rifle->AmmoPerClip);
	Shooter.Tap(World, EKeys::One, false);
	TickFrames(World, FramesFor(Rifle->EquipDuration));
	Rifle->SetSilenced(true);
	Shooter.Tap(World, EKeys::LeftMouseButton, false);
	const int32 Clip = Rifle->GetCurrentAmmoInClip();
	const int32 Reserve = Rifle->GetCurrentAmmo();
	TestTrue("Fired", Clip < Rifle->AmmoPerClip);

	Shooter.Tap(World, EKeys::G, false);
	TestTrue("G drops it", Rifle->IsDropped() && Character.GetWeaponInSlot(EShooterWeaponSlot::Primary) == nullptr);
	TestTrue("and draws the pistol", Character.GetWeapon() == Character.GetWeaponInSlot(EShooterWeaponSlot::Secondary));
	TickFrames(World, FramesFor(Rifle->PickupDelay));
	TestTrue("Out of reach where it fell", Rifle->IsDropped());

	Shooter.StandOn(Rifle->GetActorLocation());
	TickFrames(World, 2);
	TestTrue("Walking over it takes it",
		!Rifle->IsDropped() && Character.GetWeaponInSlot(EShooterWeaponSlot::Primary) == Rifle);
	TestTrue(
		"The pistol stays drawn", Character.GetWeapon() == Character.GetWeaponInSlot(EShooterWeaponSlot::Secondary));
	TestEqual("Its clip", Rifle->GetCurrentAmmoInClip(), Clip);
	TestEqual("Its reserve", Rifle->GetCurrentAmmo(), Reserve);
	TestTrue("Its silencer", Rifle->IsSilenced());
	TestEqual("The notice", Controller.GetPickupMessage(), FString(TEXT("Picked up m4a1")));
	const AShooterHUD* HUD = Shooter.PaintHUD();
	if (!TestNotNull("The HUD", HUD))
	{
		return false;
	}
	TestEqual("The HUD shows it", HUD->GetPickupNoticeText(), FString(TEXT("Picked up m4a1")));
	TickFrames(World, FramesFor(HUD->PickupNoticeDuration));
	TestTrue("and then not", Shooter.PaintHUD()->GetPickupNoticeText().IsEmpty());

	AShooterWeapon* Other = Character.GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("mp5")));
	TestTrue("A second primary drops the first", Other != nullptr && Rifle->IsDropped());
	TickFrames(World, FramesFor(Rifle->PickupDelay));
	Shooter.StandOn(Rifle->GetActorLocation());
	TickFrames(World, 2);
	TestTrue("A full slot leaves it", Rifle->IsDropped());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameInputDropAndPickUpBombTest, "ShooterGame.Input.DropAndPickUpBomb",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameInputDropAndPickUpBombTest::RunTest(const FString& Parameters)
{
	// CS's C4 in slot 5: 5 (the D-pad's down) draws the carried bomb (the weapon put away, the HUD's weapon line "C4"),
	// a weapon's key puts it away, and G drops it ahead. Its dropper cannot take it back for the bomb's PickupDelay;
	// after it, walking over it does, and the HUD says "Picked up C4". A dead carrier drops it and its rifle.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	(void)World.SetGameMode(AShooterGameMode::StaticClass());
	const FTestShooter Shooter(World);
	AShooterCharacter& Character = *Shooter.Character;
	AShooterPlayerController& Controller = *Shooter.Controller;
	if (AShooterPlayerState* State = Controller.GetPlayerState<AShooterPlayerState>())
	{
		State->SetTeam(EShooterTeam::T);
	}
	AShooterBomb* Bomb = World.SpawnActor<AShooterBomb>(Character.GetActorLocation(), FRotator::ZeroRotator);
	Bomb->GiveTo(&Character);
	AShooterWeapon* Pistol = Character.GetWeaponInSlot(EShooterWeaponSlot::Secondary);
	if (!TestTrue("The carrier", Character.GetCarriedBomb() == Bomb && Character.GetTeam() == EShooterTeam::T) ||
		!TestNotNull("A pistol", Pistol))
	{
		return false;
	}
	Shooter.Tap(World, EKeys::G, false);
	TestTrue("G drops the weapon in hand, not the bomb", Character.GetCarriedBomb() == Bomb);
	TestTrue("(the pistol)", Pistol->IsDropped());
	Shooter.StandOn(Pistol->GetActorLocation());
	TickFrames(World, FramesFor(Pistol->PickupDelay));
	TestTrue("The pistol back", Character.GetWeaponInSlot(EShooterWeaponSlot::Secondary) == Pistol);
	Shooter.Tap(World, EKeys::Two, false);

	Shooter.Tap(World, EKeys::Five, false);
	TestTrue("5 draws the bomb", Character.IsBombDrawn() && Character.GetWeapon() == nullptr);
	TestFalse("the pistol put away", Pistol->IsEquipped());
	const AShooterHUD* HUD = Shooter.PaintHUD();
	TestTrue("The HUD's weapon: C4", HUD != nullptr && HUD->GetWeaponText() == TEXT("C4"));
	Shooter.Tap(World, EKeys::Two, false);
	TestTrue("2 puts it away", !Character.IsBombDrawn() && Character.GetWeapon() == Pistol);
	Shooter.Tap(World, EKeys::Gamepad_DPad_Down, true);
	TestTrue("The D-pad's down draws it", Character.IsBombDrawn());

	Shooter.Tap(World, EKeys::G, false);
	TestTrue("G drops it", Bomb->GetBombState() == EShooterBombState::Dropped && Character.GetCarriedBomb() == nullptr);
	TestTrue("and draws the best weapon", !Character.IsBombDrawn() && Character.GetWeapon() == Pistol);
	TestTrue("Ahead of the feet",
		FVector::Dist2D(Bomb->GetActorLocation(), Character.GetActorLocation()) > Bomb->PickupRadius);
	Shooter.StandOn(Bomb->GetActorLocation());
	TickFrames(World, 2);
	TestTrue("Not its dropper at once", Bomb->GetBombState() == EShooterBombState::Dropped);
	TickFrames(World, FramesFor(Bomb->PickupDelay));
	TestTrue("Then it takes it back", Bomb->GetCarrier() == &Character && Character.GetCarriedBomb() == Bomb);
	TestEqual("The notice", Controller.GetPickupMessage(), FString(TEXT("Picked up C4")));
	HUD = Shooter.PaintHUD();
	TestTrue("The HUD shows it", HUD != nullptr && HUD->GetPickupNoticeText() == TEXT("Picked up C4"));

	AShooterWeapon* Rifle = Character.GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("ak47")));
	Shooter.Tap(World, EKeys::Five, false);
	Character.Suicide();
	TestTrue("A dead carrier drops the bomb", Bomb->GetBombState() == EShooterBombState::Dropped);
	TestTrue("and its rifle", Rifle != nullptr && Rifle->IsDropped());
	TestFalse("Nothing drawn", Character.IsBombDrawn());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
