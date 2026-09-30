#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/Level.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/AutomationTest.h"
#include "ShooterAIController.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterWeapon_Projectile.h"

#if WITH_DEV_AUTOMATION_TESTS

// ps2-shipping N30e's tests of the bots' tactics (AShooterAIController's class comment): the teams' buy plans (eco,
// force-buy, full) and the bots' purchases by them, the grenades they buy within CS's limits and throw (the way into
// the site, an enemy's spot), and how they move as they fight (the strafe, the crouch at range, the AWP standing
// still). The map is P20's small open one.

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

	/**
	 * The open test map: a floor, CT starts at X = -1500, T starts at X = 1500, their buy zones, bomb site A at the
	 * centre; short phases; NumCT and NumT bots with their brains on (created in that order: the CT first).
	 */
	AShooterGameMode* SetUpBotMatch(UWorld& World, int32 NumCT, int32 NumT)
	{
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float Y = (static_cast<float>(Index) - 2.0f) * 150.0f;
			APlayerStart* CTStart = World.SpawnActor<APlayerStart>(FVector(-1500.0f, Y, 92.0f), FRotator::ZeroRotator);
			CTStart->PlayerStartTag = FName(TEXT("CT"));
			APlayerStart* TStart =
				World.SpawnActor<APlayerStart>(FVector(1500.0f, Y, 92.0f), FRotator(0.0f, 180.0f, 0.0f));
			TStart->PlayerStartTag = FName(TEXT("T"));
		}
		auto SpawnZone = [&World](const FVector& Center, const FVector& Size, const TCHAR* Kind, const TCHAR* Name)
		{
			ATriggerVolume* Zone = World.SpawnActor<ATriggerVolume>(
				ATriggerVolume::StaticClass(), FTransform(FQuat::Identity, Center, Size / 100.0f));
			Zone->Tags.Add(FName(Kind));
			Zone->Tags.Add(FName(Name));
		};
		SpawnZone(FVector(-1500.0f, 0.0f, 150.0f), FVector(600.0f, 1000.0f, 300.0f), TEXT("BuyZone"), TEXT("CT"));
		SpawnZone(FVector(1500.0f, 0.0f, 150.0f), FVector(600.0f, 1000.0f, 300.0f), TEXT("BuyZone"), TEXT("T"));
		SpawnZone(FVector(0.0f, 0.0f, 150.0f), FVector(600.0f, 600.0f, 300.0f), TEXT("BombSite"), TEXT("A"));
		AShooterGameMode* GameMode = Cast<AShooterGameMode>(World.SetGameMode(AShooterGameMode::StaticClass()));
		GameMode->FreezeTime = 0.5f;
		GameMode->RoundTime = 60.0f;
		GameMode->RoundRestartDelay = 0.5f;
		GameMode->BuyTime = 3.0f;
		GameMode->MaxRounds = 10;
		GameMode->RandomSeed = 3;
		(void)GameMode->AddBots(EShooterTeam::CT, NumCT);
		(void)GameMode->AddBots(EShooterTeam::T, NumT);
		return GameMode;
	}

	TArray<AShooterCharacter*> GetAlive(UWorld& World, EShooterTeam Team)
	{
		TArray<AShooterCharacter*> Pawns;
		for (AActor* Actor : World.PersistentLevel->Actors)
		{
			AShooterCharacter* Pawn = Cast<AShooterCharacter>(Actor);
			if (Pawn != nullptr && !Pawn->IsPendingKillPending() && Pawn->IsAlive() && Pawn->GetTeam() == Team)
			{
				Pawns.Add(Pawn);
			}
		}
		return Pawns;
	}

	void TickUntilLive(UWorld& World, const AShooterGameMode& GameMode)
	{
		for (int32 Frame = 0; Frame < 600; ++Frame)
		{
			const AShooterGameState* State = GameMode.GetShooterGameState();
			if (State != nullptr && State->GetRoundState() == EShooterRoundState::Live)
			{
				return;
			}
			World.Tick(FrameTime);
		}
	}

	/** Ends the round and ticks into the next one's freeze (its plans decided, its buying open). */
	void NextRound(UWorld& World, AShooterGameMode& GameMode, EShooterRoundEndReason Reason)
	{
		GameMode.EndRound(Reason);
		TickFrames(World, 40);
	}

	/** A wall box of Size cm, its bottom on the floor, centred at X / Y. */
	void SpawnWall(UWorld& World, float X, float Y, const FVector& Size)
	{
		(void)World.SpawnActor<ABlockingVolume>(
			ABlockingVolume::StaticClass(), FTransform(FQuat::Identity, FVector(X, Y, Size.Z * 0.5f), Size / 100.0f));
	}

	/** Switches a bot's brain off (its pawn stays where it is put). */
	void Freeze(AShooterCharacter& Pawn)
	{
		if (AShooterAIController* Bot = Cast<AShooterAIController>(Pawn.GetController()))
		{
			Bot->SetActorTickEnabled(false);
		}
	}

	AShooterAIController* GetBot(const AShooterCharacter& Pawn)
	{
		return Cast<AShooterAIController>(Pawn.GetController());
	}

	/** Sets every bot's grenade chance before the first round draws it (its first tick). */
	void SetGrenadeChance(const AShooterGameMode& GameMode, float Chance)
	{
		for (APlayerState* State : GameMode.GetGameState().GetPlayerArray())
		{
			if (AShooterAIController* Bot = State != nullptr ? Cast<AShooterAIController>(State->GetOwner()) : nullptr)
			{
				Bot->GrenadeChance = Chance;
			}
		}
	}

	void SetMoney(const AShooterCharacter& Pawn, int32 Money, const AShooterGameMode& GameMode)
	{
		Pawn.GetController()->GetPlayerState<AShooterPlayerState>()->SetMoney(Money, GameMode.MaxMoney);
	}

	int32 CountItem(const TArray<FString>& Bought, const TCHAR* Item)
	{
		int32 Count = 0;
		for (const FString& Entry : Bought)
		{
			Count += Entry == Item ? 1 : 0;
		}
		return Count;
	}

	/** The grenades of a kind a pawn carries (0 without its weapon). */
	int32 CountGrenades(const AShooterCharacter& Pawn, const TCHAR* Name)
	{
		const AShooterWeapon* Weapon = Pawn.FindWeaponOfClass(AShooterWeapon::FindWeaponClass(Name));
		return Weapon != nullptr ? Weapon->GetCurrentAmmoInClip() : 0;
	}

	/** A radio message in the log: its sender's team and what it said (the last one that matches). */
	const FShooterRadioEntry* FindRadio(const AShooterGameMode& GameMode, EShooterRadioMessage Message)
	{
		const TArray<FShooterRadioEntry>& Log = GameMode.GetShooterGameState()->GetRadioLog();
		for (int32 Index = Log.Num() - 1; Index >= 0; --Index)
		{
			if (Log[Index].Message == Message)
			{
				return &Log[Index];
			}
		}
		return nullptr;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsEcoAndForceBuyTest, "ShooterGame.Bots.EcoAndForceBuy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsEcoAndForceBuyTest::RunTest(const FString& Parameters)
{
	// The plan's rules (AShooterGameMode::ChooseBuyPlan), then a 2v2 match (bot_stop) through three rounds: the pistol
	// round; the CT win it (a force-buy: short of the M4A1 and a helmet, a winner spends) and the terrorists, one loss
	// down and poor, save (eco) except a rich one; the terrorists lose again (two in a row: they force, a Desert Eagle
	// and kevlar) while the CT, equipped, buy in full.
	constexpr int32 ForceStreak = 2;
	TestTrue("The first round of a half: pistols",
		AShooterGameMode::ChooseBuyPlan(true, false, 3, 5, 5, ForceStreak) == EShooterBuyPlan::Pistol);
	TestTrue("Half of the team can buy in full: full",
		AShooterGameMode::ChooseBuyPlan(false, false, 1, 3, 5, ForceStreak) == EShooterBuyPlan::Full);
	TestTrue("Short of it after one loss: eco",
		AShooterGameMode::ChooseBuyPlan(false, false, 1, 2, 5, ForceStreak) == EShooterBuyPlan::Eco);
	TestTrue("After two losses: force",
		AShooterGameMode::ChooseBuyPlan(false, false, 2, 0, 5, ForceStreak) == EShooterBuyPlan::Force);
	TestTrue("After a win: force",
		AShooterGameMode::ChooseBuyPlan(false, false, 0, 1, 5, ForceStreak) == EShooterBuyPlan::Force);
	TestTrue("The half's last round: force",
		AShooterGameMode::ChooseBuyPlan(false, true, 1, 0, 5, ForceStreak) == EShooterBuyPlan::Force);

	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpBotMatch(World, 2, 2);
	GameMode->bBotStop = true;
	TickFrames(World, 1);
	TestTrue("Round 1: the pistol round",
		GameMode->GetTeamBuyPlan(EShooterTeam::CT) == EShooterBuyPlan::Pistol &&
			GameMode->GetTeamBuyPlan(EShooterTeam::T) == EShooterBuyPlan::Pistol);
	const TArray<AShooterCharacter*> CTs = GetAlive(World, EShooterTeam::CT);
	const TArray<AShooterCharacter*> Ts = GetAlive(World, EShooterTeam::T);
	if (!TestEqual("Two a side", CTs.Num() + Ts.Num(), 4))
	{
		return false;
	}
	TArray<FString> Bought = GetBot(*CTs[0])->BuyForRound();
	TestTrue("$800: kevlar", Bought.Num() == 1 && Bought[0] == TEXT("vest"));

	// The CT win: $150 + $3250 and $800 + $3250, short of $4100; the terrorists $1000 + $1400.
	SetMoney(*Ts[0], 1000, *GameMode);
	SetMoney(*Ts[1], 1000, *GameMode);
	NextRound(World, *GameMode, EShooterRoundEndReason::TargetSaved);
	TestTrue("Round 2: the winners force", GameMode->GetTeamBuyPlan(EShooterTeam::CT) == EShooterBuyPlan::Force);
	TestTrue("the losers save", GameMode->GetTeamBuyPlan(EShooterTeam::T) == EShooterBuyPlan::Eco);
	Bought = GetBot(*CTs[1])->BuyForRound();
	TestTrue("$4050: the M4A1 and kevlar", Bought.Contains(TEXT("m4a1")) && Bought.Contains(TEXT("vest")));
	TestEqual("An eco: nothing", GetBot(*Ts[0])->BuyForRound().Num(), 0);
	TestEqual("The money kept", Ts[0]->GetController()->GetPlayerState<AShooterPlayerState>()->GetMoney(), 2400);
	SetMoney(*Ts[1], 5000, *GameMode);
	Bought = GetBot(*Ts[1])->BuyForRound();
	TestTrue("A rich one buys anyway", Bought.Contains(TEXT("ak47")) && Bought.Contains(TEXT("vesthelm")));

	// The terrorists lose again (two in a row) with nothing left (the rich one's rifle gone too): they force; the CT
	// carry their rifle or have the money.
	if (AShooterWeapon* Rifle = Ts[1]->GetWeaponInSlot(EShooterWeaponSlot::Primary))
	{
		Ts[1]->RemoveWeapon(Rifle);
		(void)Rifle->Destroy();
	}
	SetMoney(*Ts[0], 0, *GameMode);
	SetMoney(*Ts[1], 0, *GameMode);
	NextRound(World, *GameMode, EShooterRoundEndReason::TargetSaved);
	TestEqual("Two losses", GameMode->GetLossStreak(EShooterTeam::T), 2);
	TestTrue("Round 3: the losers force", GameMode->GetTeamBuyPlan(EShooterTeam::T) == EShooterBuyPlan::Force);
	TestTrue("the equipped winners buy in full", GameMode->GetTeamBuyPlan(EShooterTeam::CT) == EShooterBuyPlan::Full);
	Bought = GetBot(*Ts[0])->BuyForRound();
	TestTrue("$1900 on a force-buy: a Desert Eagle and armor",
		Bought.Contains(TEXT("deagle")) && (Bought.Contains(TEXT("vest")) || Bought.Contains(TEXT("vesthelm"))) &&
			!Bought.Contains(TEXT("ak47")) && !Bought.Contains(TEXT("mp5")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsBuysGrenadesTest, "ShooterGame.Bots.BuysGrenadesWithinLimits",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsBuysGrenadesTest::RunTest(const FString& Parameters)
{
	// A rich bot buys its gun, armor and kit, then the grenades of GrenadeBuyOrder: two flashbangs, one HE, one smoke
	// (CS's limits); the next round, carrying them, it buys no more. One with $300 left gets a flashbang.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpBotMatch(World, 1, 1);
	GameMode->bBotStop = true;
	TickFrames(World, 1);
	AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	AShooterCharacter* T = GetAlive(World, EShooterTeam::T)[0];
	AShooterAIController* CTBot = GetBot(*CT);
	CTBot->AwpChance = 0.0f;
	SetMoney(*CT, 16000, *GameMode);
	TArray<FString> Bought = CTBot->BuyForRound();
	TestTrue("Its rifle, armor and kit first",
		Bought.Contains(TEXT("m4a1")) && Bought.Contains(TEXT("vesthelm")) && Bought.Contains(TEXT("defuser")));
	TestEqual("Two flashbangs", CountItem(Bought, TEXT("flashbang")), 2);
	TestEqual("One HE", CountItem(Bought, TEXT("hegrenade")), 1);
	TestEqual("One smoke", CountItem(Bought, TEXT("smokegrenade")), 1);
	TestEqual("Carried: two flashbangs", CountGrenades(*CT, TEXT("flashbang")), 2);
	TestEqual("Carried: the HE", CountGrenades(*CT, TEXT("hegrenade")), 1);
	TestEqual("Carried: the smoke", CountGrenades(*CT, TEXT("smokegrenade")), 1);
	TestTrue("A gun drawn, not a grenade", CT->GetWeapon() == CT->GetWeaponInSlot(EShooterWeaponSlot::Primary));

	// The terrorist: the AK-47, its ammunition and kevlar with a helmet leave $300 of $4040: a flashbang.
	GetBot(*T)->AwpChance = 0.0f;
	SetMoney(*T, 2500 + 240 + 1000 + 300, *GameMode);
	Bought = GetBot(*T)->BuyForRound();
	TestTrue("The rest: a flashbang",
		Bought.Contains(TEXT("ak47")) && CountItem(Bought, TEXT("flashbang")) == 1 &&
			CountItem(Bought, TEXT("hegrenade")) == 0);

	SetMoney(*CT, 16000, *GameMode);
	NextRound(World, *GameMode, EShooterRoundEndReason::Draw);
	SetMoney(*CT, 16000, *GameMode);
	Bought = CTBot->BuyForRound();
	TestTrue("The next round: no grenade past the limits",
		!Bought.Contains(TEXT("flashbang")) && !Bought.Contains(TEXT("hegrenade")) &&
			!Bought.Contains(TEXT("smokegrenade")));
	TestTrue("Still two, one and one",
		CountGrenades(*CT, TEXT("flashbang")) == 2 && CountGrenades(*CT, TEXT("hegrenade")) == 1 &&
			CountGrenades(*CT, TEXT("smokegrenade")) == 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsThrowsGrenadesTest, "ShooterGame.Bots.ThrowsGrenades",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsThrowsGrenadesTest::RunTest(const FString& Parameters)
{
	// The throw's pitch: the low arc (flat 15 m at 1500 cm/s under 980 cm/s^2: 20.4 degrees), 45 out of reach.
	TestEqual("The low arc", AShooterAIController::ComputeThrowPitch(1500.0f, 0.0f, 1500.0f, 980.0f), 20.4f, 0.1f);
	TestEqual("Out of reach", AShooterAIController::ComputeThrowPitch(3000.0f, 0.0f, 1500.0f, 980.0f), 45.0f);

	// The way in: the terrorist carrying the bomb, 15 m from site A with a flashbang, throws it at the site (its draw
	// for the round made sure), calls "Fire in the hole!", turns its back until it goes off, then plants.
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		AShooterGameMode* GameMode = SetUpBotMatch(World, 1, 1);
		SpawnWall(World, -2600.0f, 0.0f, FVector(100.0f, 1000.0f, 400.0f));
		SetGrenadeChance(*GameMode, 1.0f);
		TickFrames(World, 1);
		AShooterCharacter* T = GetAlive(World, EShooterTeam::T)[0];
		AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
		AShooterAIController* TBot = GetBot(*T);
		(void)T->GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("flashbang")));
		TickUntilLive(World, *GameMode);
		Freeze(*CT);
		CT->Reset(FVector(-3000.0f, 0.0f, 0.0f));
		FVector Site = FVector::ZeroVector;
		TestTrue("Site A", GameMode->GetBombSiteLocation(TEXT("A"), Site));
		bool bThrowing = false;
		bool bTurnedAway = false;
		for (int32 Frame = 0; Frame < 60 * 4; ++Frame)
		{
			World.Tick(FrameTime);
			bThrowing |= TBot->GetCurrentTask() == FName(TEXT("ThrowGrenade"));
			const FVector ToSite = (Site - T->GetActorLocation()).GetSafeNormal2D();
			bTurnedAway |= FindRadio(*GameMode, EShooterRadioMessage::FireInTheHole) != nullptr &&
				TBot->IsThrowingGrenade() && FVector::DotProduct(T->GetViewRotation().Vector(), ToSite) < -0.5f;
		}
		TestTrue("It threw", bThrowing);
		TestEqual("The flashbang is gone", CountGrenades(*T, TEXT("flashbang")), 0);
		TestTrue(
			"At the site", FVector::Dist2D(TBot->GetThrowTarget(), Site) <= TBot->GrenadeThrowError * UE_SQRT_2 + 1.0f);
		const FShooterRadioEntry* Call = FindRadio(*GameMode, EShooterRadioMessage::FireInTheHole);
		TestTrue("Fire in the hole!", Call != nullptr && Call->Team == EShooterTeam::T);
		TestTrue("Its back to the flash", bTurnedAway);
		TestFalse("The throw is over", TBot->IsThrowingGrenade());
	}

	// An enemy's spot: a CT tells the team where a terrorist is (behind a wall); a teammate with an HE goes to look and
	// throws it there (its draw made sure) before it gets there. The same seed throws at the same point.
	auto ThrowAtSpot = [this]()
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		AShooterGameMode* GameMode = SetUpBotMatch(World, 2, 1);
		SpawnWall(World, 300.0f, 0.0f, FVector(100.0f, 1500.0f, 400.0f));
		SetGrenadeChance(*GameMode, 1.0f);
		TickFrames(World, 1);
		const TArray<AShooterCharacter*> CTs = GetAlive(World, EShooterTeam::CT);
		AShooterCharacter* T = GetAlive(World, EShooterTeam::T)[0];
		AShooterAIController* Thrower = GetBot(*CTs[1]);
		(void)CTs[1]->GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("hegrenade")));
		TickUntilLive(World, *GameMode);
		Freeze(*T);
		Freeze(*CTs[0]);
		T->Reset(FVector(600.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
		TickFrames(World, 1);
		const FVector Spot = T->GetActorLocation();
		TestTrue("The report",
			GameMode->SendRadioMessage(CTs[0]->GetController(), EShooterRadioMessage::EnemySpotted, &Spot));
		TestTrue("The teammate goes to look",
			Thrower->GetBlackboard().GetValueAsBool(AShooterAIController::HeardEnemyKey) ||
				Thrower->GetBlackboard().GetValueAsVector(AShooterAIController::NoiseLocationKey).Equals(Spot, 1.0f));
		for (int32 Frame = 0; Frame < 60 * 3 && CountGrenades(*CTs[1], TEXT("hegrenade")) > 0; ++Frame)
		{
			World.Tick(FrameTime);
		}
		TestEqual("The HE thrown", CountGrenades(*CTs[1], TEXT("hegrenade")), 0);
		TestTrue("At the spot",
			FVector::Dist2D(Thrower->GetThrowTarget(), Spot) <= Thrower->GrenadeThrowError * UE_SQRT_2 + 1.0f);
		const FShooterRadioEntry* Call = FindRadio(*GameMode, EShooterRadioMessage::FireInTheHole);
		TestTrue("Fire in the hole!", Call != nullptr && Call->Team == EShooterTeam::CT);
		return Thrower->GetThrowTarget();
	};
	const FVector First = ThrowAtSpot();
	const FVector Second = ThrowAtSpot();
	TestTrue("The same throw again", First.Equals(Second, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsStrafeTest, "ShooterGame.Bots.StrafeCrouchAndStand",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsStrafeTest::RunTest(const FString& Parameters)
{
	// A CT bot with its pistol fights a terrorist 8 m away (who takes no damage): it strafes left and right across the
	// line to it, each way StrafeMinTime to StrafeMaxTime, the same pattern with the same seed. With a rifle at 16 m it
	// crouches and stops; with the AWP it stands still, upright.
	struct FRun
	{
		TArray<float> Directions;
		float MaxSideways = 0.0f;
		bool bRifleCrouched = false;
		bool bRifleStill = false;
		bool bAwpUpright = false;
		bool bAwpStill = false;
	};
	auto Run = [this]()
	{
		FRun Result;
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		AShooterGameMode* GameMode = SetUpBotMatch(World, 1, 1);
		TickUntilLive(World, *GameMode);
		AShooterCharacter* T = GetAlive(World, EShooterTeam::T)[0];
		AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
		AShooterAIController* Bot = GetBot(*CT);
		Freeze(*T);
		T->SetGodMode(true);
		T->Reset(FVector(-700.0f, CT->GetActorLocation().Y, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
		const float StartY = CT->GetActorLocation().Y;
		for (int32 Frame = 0; Frame < 60 * 4; ++Frame)
		{
			World.Tick(FrameTime);
			if (Bot->GetCurrentTask() == FName(TEXT("Engage")))
			{
				Result.Directions.Add(Bot->GetStrafeDirection());
				Result.MaxSideways = FMath::Max(Result.MaxSideways, FMath::Abs(CT->GetActorLocation().Y - StartY));
			}
		}
		// A rifle, the terrorist 16 m away.
		(void)CT->GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("ak47")));
		T->Reset(
			FVector(CT->GetActorLocation().X + 1600.0f, CT->GetActorLocation().Y, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
		TickFrames(World, 60);
		Result.bRifleCrouched = CT->bIsCrouched && Bot->GetCurrentTask() == FName(TEXT("Engage"));
		Result.bRifleStill = Bot->GetStrafeDirection() == 0.0f;
		// The AWP instead.
		(void)CT->GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("awp")));
		TickFrames(World, 60);
		Result.bAwpUpright =
			!CT->bIsCrouched && CT->GetWeapon() != nullptr && CT->GetWeapon()->WeaponName == TEXT("awp");
		Result.bAwpStill = Bot->GetStrafeDirection() == 0.0f && CT->GetCharacterMovement().Velocity.Size2D() < 5.0f;
		return Result;
	};
	const FRun First = Run();
	const FRun Second = Run();
	TestTrue("It engaged", First.Directions.Num() > 60);
	// The frames between two changes of side: a whole leg each (the first and the last are cut by the recording).
	int32 Lefts = 0;
	int32 Rights = 0;
	TArray<int32> Changes;
	for (int32 Index = 0; Index < First.Directions.Num(); ++Index)
	{
		Lefts += First.Directions[Index] < 0.0f ? 1 : 0;
		Rights += First.Directions[Index] > 0.0f ? 1 : 0;
		if (Index > 0 && First.Directions[Index] != First.Directions[Index - 1])
		{
			Changes.Add(Index);
		}
	}
	TestTrue("Both ways", Lefts > 0 && Rights > 0 && Changes.Num() >= 3);
	const AShooterAIController* Defaults = GetDefault<AShooterAIController>();
	for (int32 Index = 1; Index < Changes.Num(); ++Index)
	{
		const float Leg = static_cast<float>(Changes[Index] - Changes[Index - 1]) * FrameTime;
		TestTrue(
			*FString::Printf(TEXT("A leg of %.2f s within StrafeMinTime and StrafeMaxTime"), static_cast<double>(Leg)),
			Leg >= Defaults->StrafeMinTime - (2.0f * FrameTime) && Leg <= Defaults->StrafeMaxTime + (2.0f * FrameTime));
	}
	TestTrue("It moved sideways", First.MaxSideways > 50.0f);
	TestTrue("The same pattern with the same seed", First.Directions == Second.Directions);
	TestTrue("A rifle at range: crouched", First.bRifleCrouched);
	TestTrue("and no strafe", First.bRifleStill);
	TestTrue("The AWP: upright", First.bAwpUpright);
	TestTrue("and still", First.bAwpStill);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
