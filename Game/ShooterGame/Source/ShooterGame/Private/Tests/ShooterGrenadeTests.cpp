#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "ShooterAIController.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterPawnSensingComponent.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"
#include "Weapons/ShooterProjectile.h"
#include "Weapons/ShooterSmokeCloud.h"
#include "Weapons/ShooterWeapon_Projectile.h"

#if WITH_DEV_AUTOMATION_TESTS

// ps2-shipping N30b's tests: the flashbang (how white and how long by the view's angle, the distance and the walls;
// the bots it blinds), the smoke grenade (its cloud hides from the bots' sight and goes), and the grenades a player
// carries (CS: two flashbangs, one HE, one smoke; the grenade key cycles them).

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

	void TickSeconds(UWorld& World, float Seconds)
	{
		TickFrames(World, FMath::CeilToInt(Seconds / FrameTime) + 1);
	}

	/** A floor under the origin and the game mode (its registries and player states). */
	void SetUpWorld(UWorld& World)
	{
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
		(void)World.SetGameMode(AShooterGameMode::StaticClass());
	}

	/** A wall box of Size cm centred at Center. */
	void SpawnWall(UWorld& World, const FVector& Center, const FVector& Size)
	{
		(void)World.SpawnActor<ABlockingVolume>(
			ABlockingVolume::StaticClass(), FTransform(FQuat::Identity, Center, Size / 100.0f));
	}

	/** A character at Feet facing Yaw, possessed by a bot controller of Team, its brain off. */
	AShooterCharacter* SpawnShooter(UWorld& World, const FVector& Feet, float Yaw, EShooterTeam Team)
	{
		AShooterCharacter* Character = World.SpawnActor<AShooterCharacter>(Feet, FRotator(0.0f, Yaw, 0.0f));
		AShooterAIController* Controller = World.SpawnActor<AShooterAIController>();
		Controller->SetActorTickEnabled(false);
		if (AShooterPlayerState* State = Controller->GetPlayerState<AShooterPlayerState>())
		{
			State->SetTeam(Team);
		}
		Controller->Possess(Character);
		Controller->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
		return Character;
	}

	/** A projectile of Class set off at Location (a flashbang, a smoke grenade), thrown by Thrower. */
	template <class T>
	T* Detonate(UWorld& World, const FVector& Location, AShooterCharacter* Thrower)
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Instigator = Thrower;
		T* Projectile = World.SpawnActor<T>(Location, FRotator::ZeroRotator, SpawnInfo);
		Projectile->Launch(FVector::ZeroVector, Thrower != nullptr ? Thrower->GetController() : nullptr, nullptr);
		Projectile->Explode();
		return Projectile;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameGrenadesFlashIntensityTest, "ShooterGame.Grenades.FlashIntensity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameGrenadesFlashIntensityTest::RunTest(const FString& Parameters)
{
	// CS 1.6's RadiusFlash: the strength falls from 4 at the flash to nothing at 1500 units (38.1 m); looking at it the
	// screen holds white 1.5 x the strength seconds and fades in 3 x; looking aside 0.45 and 1.75 at 200 of 255;
	// behind 0.2 and 1; a wall in between (or the range) spares the player. The thrower's teammates are flashed too.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SetUpWorld(World);
	const FVector Origin(0.0f, 0.0f, 100.0f);
	auto Facing = [&Origin](const FVector& Feet) { return (Origin - Feet).Rotation().Yaw; };
	AShooterCharacter* Looking = SpawnShooter(World, FVector(500.0f, 0.0f, 0.0f), 180.0f, EShooterTeam::T);
	AShooterCharacter* Aside = SpawnShooter(World, FVector(0.0f, 500.0f, 0.0f), 0.0f, EShooterTeam::T);
	AShooterCharacter* Away = SpawnShooter(World, FVector(-500.0f, 0.0f, 0.0f), 180.0f, EShooterTeam::CT);
	AShooterCharacter* Farther = SpawnShooter(World, FVector(0.0f, 2000.0f, 0.0f), -90.0f, EShooterTeam::CT);
	AShooterCharacter* OutOfRange = SpawnShooter(World, FVector(4200.0f, 0.0f, 0.0f), 180.0f, EShooterTeam::T);
	const FVector WalledFeet(300.0f, -600.0f, 0.0f);
	AShooterCharacter* Walled = SpawnShooter(World, WalledFeet, Facing(WalledFeet), EShooterTeam::T);
	SpawnWall(World, FVector(150.0f, -300.0f, 150.0f), FVector(300.0f, 40.0f, 300.0f));
	TickFrames(World, 2);

	const AShooterProjectile_Flashbang* Defaults = GetDefault<AShooterProjectile_Flashbang>();
	auto Expect = [this, Defaults, &Origin](
					  const TCHAR* Name, const AShooterCharacter& Pawn, float HoldScale, float FadeScale, float Alpha)
	{
		const FVector Eyes = Pawn.GetFirstPersonCameraComponent()->GetComponentLocation();
		const float Strength = Defaults->FlashStrength * (1.0f - (FVector::Dist(Origin, Eyes) / Defaults->FlashRadius));
		TestEqual(*FString::Printf(TEXT("%s: the hold"), Name), Pawn.GetFlashHoldTime(), Strength * HoldScale, 1.0e-3f);
		TestEqual(*FString::Printf(TEXT("%s: the fade"), Name), Pawn.GetFlashFadeTime(), Strength * FadeScale, 1.0e-3f);
		TestEqual(*FString::Printf(TEXT("%s: how white"), Name), Pawn.GetFlashAlpha(), Alpha, 1.0e-3f);
	};
	// The explosion is 10 cm over the grenade.
	(void)Detonate<AShooterProjectile_Flashbang>(World, Origin - FVector(0.0f, 0.0f, 10.0f), Away);
	Expect(TEXT("Looking at it"), *Looking, 1.5f, 3.0f, 1.0f);
	Expect(TEXT("Aside"), *Aside, 0.45f, 1.75f, 200.0f / 255.0f);
	Expect(TEXT("Away"), *Away, 0.2f, 1.0f, 200.0f / 255.0f);
	Expect(TEXT("Farther, looking at it"), *Farther, 1.5f, 3.0f, 1.0f);
	TestTrue("Farther is shorter", Farther->GetFlashHoldTime() < Looking->GetFlashHoldTime());
	TestEqual("Out of range", OutOfRange->GetFlashAlpha(), 0.0f);
	TestEqual("Behind a wall", Walled->GetFlashAlpha(), 0.0f);

	// The white holds, then fades to nothing.
	TickSeconds(World, Looking->GetFlashHoldTime() - 0.1f);
	TestEqual("Still white", Looking->GetFlashAlpha(), 1.0f, 1.0e-3f);
	TickSeconds(World, Looking->GetFlashFadeTime() * 0.5f);
	TestTrue("Fading", Looking->GetFlashAlpha() > 0.3f && Looking->GetFlashAlpha() < 0.7f);
	TickSeconds(World, (Looking->GetFlashFadeTime() * 0.5f) + 0.2f);
	TestEqual("Gone", Looking->GetFlashAlpha(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameGrenadesFlashBlindsBotsTest, "ShooterGame.Grenades.FlashBlindsBots",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameGrenadesFlashBlindsBotsTest::RunTest(const FString& Parameters)
{
	// A flashed bot is blind a third of the fade (CS's Blind): it sees nobody (its sensing traces nothing) and, as CS's
	// bots do (ps2-shipping N30e), stands and fires at random around where it last saw its enemy, from its stream: the
	// same shots and aim with the same seed. Then it sees the enemy again and engages it.
	struct FBlindRun
	{
		int32 BlindShots = 0;
		float MaxYawOff = 0.0f;
		FRotator Aim = FRotator::ZeroRotator;
	};
	auto Run = [this]()
	{
		FBlindRun Result;
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		SetUpWorld(World);
		AShooterCharacter* Bot = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
		AShooterCharacter* Enemy = SpawnShooter(World, FVector(1500.0f, 0.0f, 0.0f), 180.0f, EShooterTeam::T);
		Enemy->SetGodMode(true);
		AShooterAIController* Brain = Cast<AShooterAIController>(Bot->GetController());
		if (!TestNotNull("A bot", Brain))
		{
			return Result;
		}
		Brain->SetActorTickEnabled(true);
		UShooterPawnSensingComponent* Sensing = Brain->GetPawnSensing();
		TestTrue("It sees the enemy", Sensing->CouldSeePawn(Enemy));
		for (int32 Frame = 0; Frame < 30 && Brain->GetEnemy() != Enemy; ++Frame)
		{
			TickFrames(World, 1);
		}
		TestTrue("It saw it", Brain->GetEnemy() == Enemy);

		(void)Detonate<AShooterProjectile_Flashbang>(World, FVector(300.0f, 0.0f, 150.0f), Enemy);
		const float BlindTime = Bot->GetFlashFadeTime() * GetDefault<AShooterProjectile_Flashbang>()->BotBlindFadeShare;
		TestTrue("Blind", Bot->IsBlind() && BlindTime > 1.0f);
		const int32 TracesBefore = Sensing->GetNumSightTraces();
		const AShooterWeapon* Weapon = Bot->GetWeapon();
		const int32 ShotsBefore = Weapon != nullptr ? Weapon->GetShotsFired() : 0;
		const int32 BlindFrames = FMath::FloorToInt(BlindTime * 0.9f / FrameTime);
		for (int32 Frame = 0; Frame < BlindFrames; ++Frame)
		{
			TickFrames(World, 1);
			Result.MaxYawOff =
				FMath::Max(Result.MaxYawOff, FMath::Abs(FRotator::NormalizeAxis(Brain->GetControlRotation().Yaw)));
		}
		TestEqual("It traces to nobody", Sensing->GetNumSightTraces(), TracesBefore);
		TestTrue("It fires blind", Brain->GetCurrentTask() == FName(TEXT("Blind")));
		Result.BlindShots = Bot->GetWeapon() != nullptr ? Bot->GetWeapon()->GetShotsFired() - ShotsBefore : 0;
		Result.Aim = Brain->GetControlRotation();
		TickSeconds(World, BlindTime * 0.1f + 0.5f);
		TestFalse("It sees again", Bot->IsBlind());
		TestTrue("And engages", Brain->GetEnemy() == Enemy);
		return Result;
	};
	const FBlindRun First = Run();
	const FBlindRun Second = Run();
	TestTrue("Shots while blind", First.BlindShots > 0);
	TestTrue(*FString::Printf(TEXT("Around where it saw the enemy (%.1f degrees off at most)"),
				 static_cast<double>(First.MaxYawOff)),
		First.MaxYawOff <= GetDefault<AShooterAIController>()->BlindFireError + 1.0f);
	TestTrue("The same blind fire with the same seed",
		First.BlindShots == Second.BlindShots && First.Aim.Equals(Second.Aim, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameGrenadesSmokeTest, "ShooterGame.Grenades.SmokeBlocksSight",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameGrenadesSmokeTest::RunTest(const FString& Parameters)
{
	// A smoke grenade's cloud between a bot and its enemy hides the enemy (the line of sight through its sphere
	// fails) for about CS's 18 s, drawn as a handful of puffs (the world's effect sprites), which fade; then the bot
	// sees again, the cloud is gone and so are its puffs.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SetUpWorld(World);
	AShooterCharacter* Bot = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterCharacter* Enemy = SpawnShooter(World, FVector(1500.0f, 0.0f, 0.0f), 180.0f, EShooterTeam::T);
	AShooterAIController* Brain = Cast<AShooterAIController>(Bot->GetController());
	AShooterGameMode* GameMode = World.GetAuthGameMode<AShooterGameMode>();
	if (!TestNotNull("A bot", Brain) || !TestNotNull("The game mode", GameMode))
	{
		return false;
	}
	UShooterPawnSensingComponent* Sensing = Brain->GetPawnSensing();
	TestTrue("In sight", Sensing->HasLineOfSightTo(Enemy));

	(void)Detonate<AShooterProjectile_Smoke>(World, FVector(750.0f, 0.0f, 5.0f), Enemy);
	TickFrames(World, 1);
	if (!TestEqual("A cloud", GameMode->GetSmokeClouds().Num(), 1))
	{
		return false;
	}
	AShooterSmokeCloud* Cloud = GameMode->GetSmokeClouds()[0];
	TestEqual("Its puffs", World.EffectSprites.Num(), Cloud->NumPuffs);
	TestTrue("Few primitives", Cloud->NumPuffs <= 8);
	TestFalse("Hidden in the smoke", Sensing->HasLineOfSightTo(Enemy));
	TestFalse("So not seen", Sensing->CouldSeePawn(Enemy));
	TestTrue("A line beside it passes",
		!GameMode->IsSightBlockedBySmoke(FVector(0.0f, 2000.0f, 160.0f), FVector(1500.0f, 2000.0f, 160.0f)));
	TickSeconds(World, Cloud->Duration - Cloud->FadeOutTime);
	TestTrue("Still thick before it fades", !Sensing->HasLineOfSightTo(Enemy) && Cloud->IsThick());
	TickSeconds(World, Cloud->FadeOutTime);
	TestTrue("Gone after its time", Sensing->HasLineOfSightTo(Enemy) && GameMode->GetSmokeClouds().Num() == 0);
	TestEqual("With its puffs", World.EffectSprites.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
