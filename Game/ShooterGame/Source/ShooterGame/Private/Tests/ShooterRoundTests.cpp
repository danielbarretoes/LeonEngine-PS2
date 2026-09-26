#include "CanvasTypes.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/DamageEvents.h"
#include "Engine/Level.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "ShooterAIController.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterHUD.h"
#include "ShooterMatchChecker.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"
#include "Weapons/ShooterProjectile.h"
#include "Weapons/ShooterWeapon_Instant.h"
#include "Weapons/ShooterWeapon_Sniper.h"

#if WITH_DEV_AUTOMATION_TESTS

// P19's tests: the match and its rounds (the phases, the win conditions, the match's end and its restart), the money,
// buying, and the bomb (plant, defuse, explosion). Every test builds a small map: a floor, five starts a team with a
// buy zone around each team's starts, and bomb site A between them; the game mode's times are short.

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

	/** Ticks until Seconds of world time have passed. */
	void TickSeconds(UWorld& World, float Seconds)
	{
		TickFrames(World, FMath::CeilToInt(Seconds / FrameTime) + 1);
	}

	/** A trigger volume of Size cm centred at Center, tagged Kind and Name. */
	ATriggerVolume* SpawnZone(
		UWorld& World, const FVector& Center, const FVector& Size, const TCHAR* Kind, const TCHAR* Name)
	{
		ATriggerVolume* Zone = World.SpawnActor<ATriggerVolume>(
			ATriggerVolume::StaticClass(), FTransform(FQuat::Identity, Center, Size / 100.0f));
		Zone->Tags.Add(FName(Kind));
		Zone->Tags.Add(FName(Name));
		return Zone;
	}

	/**
	 * The test map: a floor 80 m square, CT starts at X = -1500 and T starts at X = 1500 (five each along Y, 150 cm
	 * apart) in their buy zones, bomb site A 6 m square at the centre. The game mode's times: freeze 0.5 s, round 20 s,
	 * result 0.5 s, buy 3 s.
	 */
	AShooterGameMode* SetUpMatch(UWorld& World, int32 NumCT, int32 NumT)
	{
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float Y = (static_cast<float>(Index) - 2.0f) * 150.0f;
			APlayerStart* CTStart =
				World.SpawnActor<APlayerStart>(FVector(-1500.0f, Y, 92.0f), FRotator(0.0f, 0.0f, 0.0f));
			CTStart->PlayerStartTag = FName(TEXT("CT"));
			APlayerStart* TStart =
				World.SpawnActor<APlayerStart>(FVector(1500.0f, Y, 92.0f), FRotator(0.0f, 180.0f, 0.0f));
			TStart->PlayerStartTag = FName(TEXT("T"));
		}
		(void)SpawnZone(
			World, FVector(-1500.0f, 0.0f, 150.0f), FVector(600.0f, 1000.0f, 300.0f), TEXT("BuyZone"), TEXT("CT"));
		(void)SpawnZone(
			World, FVector(1500.0f, 0.0f, 150.0f), FVector(600.0f, 1000.0f, 300.0f), TEXT("BuyZone"), TEXT("T"));
		(void)SpawnZone(
			World, FVector(0.0f, 0.0f, 150.0f), FVector(600.0f, 600.0f, 300.0f), TEXT("BombSite"), TEXT("A"));

		AShooterGameMode* GameMode = Cast<AShooterGameMode>(World.SetGameMode(AShooterGameMode::StaticClass()));
		GameMode->bFillTeamsWithBots = false;
		// The rules alone: the bots stand still (bot_stop; P20's bots have their own tests).
		GameMode->bBotStop = true;
		GameMode->FreezeTime = 0.5f;
		GameMode->RoundTime = 20.0f;
		GameMode->RoundRestartDelay = 0.5f;
		GameMode->BuyTime = 3.0f;
		GameMode->MaxRounds = 30;
		GameMode->RandomSeed = 7;
		(void)GameMode->AddBots(EShooterTeam::CT, NumCT);
		(void)GameMode->AddBots(EShooterTeam::T, NumT);
		return GameMode;
	}

	/** The live shooter pawns of a team, in level order. */
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

	/** The player states of a team. */
	TArray<AShooterPlayerState*> GetTeamStates(const AShooterGameMode& GameMode, EShooterTeam Team)
	{
		TArray<AShooterPlayerState*> States;
		for (APlayerState* State : GameMode.GetGameState().GetPlayerArray())
		{
			AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(State);
			if (ShooterState != nullptr && ShooterState->GetTeam() == Team)
			{
				States.Add(ShooterState);
			}
		}
		return States;
	}

	/** Ticks until the round is Live (the match started, the freeze over). */
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

	/** Kills every live pawn of a team. */
	void KillTeam(UWorld& World, EShooterTeam Team)
	{
		for (AShooterCharacter* Pawn : GetAlive(World, Team))
		{
			Pawn->Suicide();
		}
	}

	/** Moves the bomb's carrier into site A and plants: returns the planter (null without one). */
	AShooterCharacter* PlantInSiteA(UWorld& World, AShooterGameMode& GameMode)
	{
		AShooterBomb* Bomb = GameMode.GetBomb();
		AShooterCharacter* Carrier = Bomb != nullptr ? Bomb->GetCarrier() : nullptr;
		if (Carrier == nullptr)
		{
			return nullptr;
		}
		Carrier->Reset(FVector(100.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
		TickFrames(World, 2);
		if (!Carrier->StartUse())
		{
			return nullptr;
		}
		TickSeconds(World, Bomb->PlantDuration);
		return Carrier;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRoundsMatchFlowTest, "ShooterGame.Rounds.MatchFlow",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRoundsMatchFlowTest::RunTest(const FString& Parameters)
{
	// Both teams in: the match starts with round 1's freeze (nobody moves, the bomb with a terrorist), then Live; the
	// terrorists die: the CT win and are paid, the losers get the loss bonus, the bomb falls; the next round respawns
	// the dead with a pistol, keeps the survivors, cleans the map and hands out a new bomb.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 2, 2);
	AShooterGameState* State = GameMode->GetShooterGameState();
	if (!TestNotNull("The game's state", State))
	{
		return false;
	}
	TestTrue("Warmup before the first tick", State->GetRoundState() == EShooterRoundState::Warmup);
	TickFrames(World, 1);
	TestTrue("The match started", GameMode->IsMatchInProgress());
	TestEqual("Round 1", State->GetRoundNumber(), 1);
	TestTrue("Freeze", State->IsFreezeTime());
	TestEqual("Two CT", GetAlive(World, EShooterTeam::CT).Num(), 2);
	TestEqual("Two T", GetAlive(World, EShooterTeam::T).Num(), 2);
	TestTrue("The bomb is carried", State->GetBombState() == EShooterBombState::Carried);
	AShooterBomb* FirstBomb = GameMode->GetBomb();
	TestTrue("By a terrorist",
		FirstBomb != nullptr && FirstBomb->GetCarrier() != nullptr &&
			FirstBomb->GetCarrier()->GetTeam() == EShooterTeam::T);
	for (const AShooterPlayerState* PlayerState : GetTeamStates(*GameMode, EShooterTeam::CT))
	{
		TestEqual("Start money", PlayerState->GetMoney(), 800);
	}

	AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	const FVector Before = CT->GetActorLocation();
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		CT->MoveForward(1.0f);
		World.Tick(FrameTime);
	}
	TestTrue("Frozen", CT->GetActorLocation().Equals(Before, 0.01f));

	TickUntilLive(World, *GameMode);
	TestTrue("Live", State->GetRoundState() == EShooterRoundState::Live);
	CT->SetArmor(50.0f, false);
	(void)CT->TakeDamage(30.0f, FDamageEvent(UDamageType::StaticClass()), nullptr, nullptr);
	KillTeam(World, EShooterTeam::T);
	TickFrames(World, 1);
	TestTrue("Round over", State->GetRoundState() == EShooterRoundState::RoundEnd);
	TestTrue("Terrorists eliminated", State->GetLastRoundEndReason() == EShooterRoundEndReason::TerroristsEliminated);
	TestEqual("CT 1", State->GetTeamScore(EShooterTeam::CT), 1);
	TestEqual("T 0", State->GetTeamScore(EShooterTeam::T), 0);
	TestTrue("The bomb fell", State->GetBombState() == EShooterBombState::Dropped);
	for (const AShooterPlayerState* PlayerState : GetTeamStates(*GameMode, EShooterTeam::CT))
	{
		TestEqual("CT: 800 + 3250", PlayerState->GetMoney(), 4050);
	}
	for (const AShooterPlayerState* PlayerState : GetTeamStates(*GameMode, EShooterTeam::T))
	{
		TestEqual("T: 800 + 1400", PlayerState->GetMoney(), 2200);
		TestEqual("A death each", PlayerState->GetDeaths(), 1);
	}

	TickSeconds(World, GameMode->RoundRestartDelay);
	TestEqual("Round 2", State->GetRoundNumber(), 2);
	TestTrue("Freeze again", State->IsFreezeTime());
	TestEqual("Two T again", GetAlive(World, EShooterTeam::T).Num(), 2);
	TestTrue("The CT survived", !CT->IsPendingKillPending() && CT->IsAlive());
	TestEqual("Full health", CT->GetHealth(), 100.0f);
	TestTrue("The armor kept", CT->GetArmor() > 0.0f);
	TestTrue("The old bomb is gone", FirstBomb->IsPendingKillPending());
	TestTrue("A new bomb",
		GameMode->GetBomb() != nullptr && GameMode->GetBomb() != FirstBomb &&
			State->GetBombState() == EShooterBombState::Carried);
	for (AShooterCharacter* T : GetAlive(World, EShooterTeam::T))
	{
		TestTrue("A respawned T has the pistol", T->GetWeaponInSlot(EShooterWeaponSlot::Secondary) != nullptr);
		TestEqual("On its side", T->GetActorLocation().X, 1500.0f, 1.0f);
	}
	int32 Corpses = 0;
	for (AActor* Actor : World.PersistentLevel->Actors)
	{
		const AShooterCharacter* Pawn = Cast<AShooterCharacter>(Actor);
		Corpses += Pawn != nullptr && !Pawn->IsPendingKillPending() && !Pawn->IsAlive() ? 1 : 0;
	}
	TestEqual("No corpses", Corpses, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRoundsWinMatrixTest, "ShooterGame.Rounds.WinMatrix",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRoundsWinMatrixTest::RunTest(const FString& Parameters)
{
	// Each way a round ends: the time with no plant (CT), the CT dead (T), both teams dead at once (a draw), the bomb's
	// explosion (T, even with every terrorist dead after the plant), a defuse (CT), and the CT dead after a plant (T).
	struct FScenario
	{
		const TCHAR* Name;
		EShooterRoundEndReason Expected;
	};
	const FScenario Scenarios[] = {
		{TEXT("Time"), EShooterRoundEndReason::TargetSaved},
		{TEXT("CT dead"), EShooterRoundEndReason::CTsEliminated},
		{TEXT("Both dead"), EShooterRoundEndReason::Draw},
		{TEXT("Bomb"), EShooterRoundEndReason::TargetBombed},
		{TEXT("Defuse"), EShooterRoundEndReason::BombDefused},
		{TEXT("CT dead after the plant"), EShooterRoundEndReason::CTsEliminated},
	};
	for (const FScenario& Scenario : Scenarios)
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		AShooterGameMode* GameMode = SetUpMatch(World, 1, 2);
		AShooterGameState* State = GameMode->GetShooterGameState();
		TickUntilLive(World, *GameMode);
		const FString Name(Scenario.Name);
		switch (Scenario.Expected)
		{
			case EShooterRoundEndReason::TargetSaved:
				TickSeconds(World, GameMode->RoundTime);
				break;
			case EShooterRoundEndReason::CTsEliminated:
				if (Name.Contains(TEXT("plant")))
				{
					TestNotNull(*(Name + TEXT(": planted")), PlantInSiteA(World, *GameMode));
				}
				KillTeam(World, EShooterTeam::CT);
				TickFrames(World, 1);
				break;
			case EShooterRoundEndReason::Draw:
			{
				// One explosion kills all three in the same moment.
				const TArray<AActor*> Ignore;
				for (const EShooterTeam Team : {EShooterTeam::CT, EShooterTeam::T})
				{
					for (AShooterCharacter* Pawn : GetAlive(World, Team))
					{
						Pawn->Reset(
							FVector(0.0f, 0.0f, 0.0f) + FVector(0.0f, Team == EShooterTeam::T ? 90.0f : -90.0f, 0.0f));
					}
				}
				TArray<AShooterCharacter*> Everyone = GetAlive(World, EShooterTeam::CT);
				Everyone.Append(GetAlive(World, EShooterTeam::T));
				for (AShooterCharacter* Pawn : Everyone)
				{
					(void)UGameplayStatics::ApplyDamage(Pawn, 500.0f, nullptr, nullptr, UDamageType::StaticClass());
				}
				TickFrames(World, 1);
				break;
			}
			case EShooterRoundEndReason::TargetBombed:
			{
				TestNotNull(*(Name + TEXT(": planted")), PlantInSiteA(World, *GameMode));
				TestTrue(*(Name + TEXT(": the clock stops")), State->GetBombState() == EShooterBombState::Planted);
				KillTeam(World, EShooterTeam::T);
				TickFrames(World, 1);
				TestTrue(
					*(Name + TEXT(": goes on with the T dead")), State->GetRoundState() == EShooterRoundState::Live);
				// Past the round's own time: the bomb decides.
				TickSeconds(World, GameMode->GetBomb()->BombTimer);
				break;
			}
			case EShooterRoundEndReason::BombDefused:
			{
				TestNotNull(*(Name + TEXT(": planted")), PlantInSiteA(World, *GameMode));
				AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
				CT->SetDefuseKit(true);
				CT->Reset(GameMode->GetBomb()->GetActorLocation() - FVector(60.0f, 0.0f, 0.0f));
				TickFrames(World, 1);
				TestTrue(*(Name + TEXT(": defusing")), CT->StartUse() && CT->IsDefusing());
				TickSeconds(World, GameMode->GetBomb()->DefuseKitDuration);
				break;
			}
			case EShooterRoundEndReason::TerroristsEliminated:
			case EShooterRoundEndReason::None:
				break;
		}
		TestTrue(*(Name + TEXT(": round over")), State->GetRoundState() == EShooterRoundState::RoundEnd);
		TestTrue(*FString::Printf(TEXT("%s: %s"), *Name, GetRoundEndMessage(Scenario.Expected)),
			State->GetLastRoundEndReason() == Scenario.Expected);
		const EShooterTeam Winner = GetRoundEndWinner(Scenario.Expected);
		TestEqual(
			*(Name + TEXT(": CT score")), State->GetTeamScore(EShooterTeam::CT), Winner == EShooterTeam::CT ? 1 : 0);
		TestEqual(*(Name + TEXT(": T score")), State->GetTeamScore(EShooterTeam::T), Winner == EShooterTeam::T ? 1 : 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameEconomyTest, "ShooterGame.Economy.RewardsAndLossBonus",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameEconomyTest::RunTest(const FString& Parameters)
{
	// CS 1.6's money: the losers' bonus climbs 1400, 1900, 2400, 2900, 3400 and stays; the winners get 3250; nobody
	// passes 16000. A kill pays the weapon's reward (CS 1.6: 300 with any), a team kill costs 3300.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	AShooterPlayerState* CTState = GetTeamStates(*GameMode, EShooterTeam::CT)[0];
	AShooterPlayerState* TState = GetTeamStates(*GameMode, EShooterTeam::T)[0];
	const int32 ExpectedT[] = {2200, 4100, 6500, 9400, 12800, 16000};
	const int32 ExpectedCT[] = {4050, 7300, 10550, 13800, 16000, 16000};
	for (int32 Round = 0; Round < 6; ++Round)
	{
		TickUntilLive(World, *GameMode);
		GameMode->EndRound(EShooterRoundEndReason::TargetSaved);
		TestEqual(*FString::Printf(TEXT("T after loss %d"), Round + 1), TState->GetMoney(), ExpectedT[Round]);
		TestEqual(*FString::Printf(TEXT("CT after win %d"), Round + 1), CTState->GetMoney(), ExpectedCT[Round]);
		TickSeconds(World, GameMode->RoundRestartDelay);
	}
	TestEqual("The streak", GameMode->GetLossStreak(EShooterTeam::T), 6);
	TestEqual("The next loss pays the most", GameMode->GetLossBonus(EShooterTeam::T), 3400);

	// Kills: the AWP pays 300 too (CS 1.6).
	TickUntilLive(World, *GameMode);
	CTState->SetMoney(1000, GameMode->MaxMoney);
	AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	AShooterCharacter* T = GetAlive(World, EShooterTeam::T)[0];
	AShooterWeapon* Awp = CT->GiveWeapon(AShooterWeapon_Sniper::StaticClass());
	(void)UGameplayStatics::ApplyDamage(T, 500.0f, CT->GetController(), Awp, UDamageType::StaticClass());
	TestFalse("Killed", T->IsAlive());
	TestEqual("The AWP's kill reward", CTState->GetMoney(), 1300);
	TestEqual("A kill", CTState->GetKills(), 1);

	// A team kill (friendly fire on) costs 3300 and a kill.
	TickSeconds(World, GameMode->RoundRestartDelay + 0.1f);
	GameMode->bFriendlyFire = true;
	(void)GameMode->AddBots(EShooterTeam::CT, 1);
	TickSeconds(World, GameMode->RoundRestartDelay);
	TickUntilLive(World, *GameMode);
	TArray<AShooterCharacter*> CTs = GetAlive(World, EShooterTeam::CT);
	if (TestEqual("Two CT", CTs.Num(), 2))
	{
		AShooterPlayerState* Killer = CTs[0]->GetController()->GetPlayerState<AShooterPlayerState>();
		Killer->SetMoney(5000, GameMode->MaxMoney);
		const int32 KillsBefore = Killer->GetKills();
		(void)UGameplayStatics::ApplyDamage(CTs[1], 500.0f, CTs[0]->GetController(),
			CTs[0]->GetWeaponInSlot(EShooterWeaponSlot::Secondary), UDamageType::StaticClass());
		TestEqual("Team kill penalty", Killer->GetMoney(), 1700);
		TestEqual("A kill taken away", Killer->GetKills(), KillsBefore - 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBombPlantAndDefuseTest, "ShooterGame.Bomb.PlantAndDefuse",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBombPlantAndDefuseTest::RunTest(const FString& Parameters)
{
	// Planting needs the site and stillness and takes 3 s (300 to the planter); a defuse needs the bomb's reach, takes
	// 10 s (5 with a kit) and stops when the defuser walks off; the planted T lose with the plant bonus.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	AShooterGameState* State = GameMode->GetShooterGameState();
	TickUntilLive(World, *GameMode);
	AShooterBomb* Bomb = GameMode->GetBomb();
	AShooterCharacter* T = Bomb != nullptr ? Bomb->GetCarrier() : nullptr;
	if (!TestNotNull("A carrier", T))
	{
		return false;
	}
	TestTrue("It carries it", T->GetCarriedBomb() == Bomb);
	TestFalse("Not outside a site", T->StartUse());

	T->Reset(FVector(100.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	TickFrames(World, 2);
	TestTrue("In site A", T->GetBombSiteHere() == FName(TEXT("A")));
	TestTrue("Planting", T->StartUse() && T->IsPlanting());
	TickSeconds(World, 1.0f);
	T->GetCharacterMovement().Velocity = FVector(300.0f, 0.0f, 0.0f);
	TickFrames(World, 1);
	TestFalse("Moving stops the plant", T->IsPlanting());
	TickFrames(World, 30);
	AShooterPlayerState* TState = T->GetController()->GetPlayerState<AShooterPlayerState>();
	const int32 TMoney = TState->GetMoney();
	TestTrue("Planting again", T->StartUse());
	TickSeconds(World, Bomb->PlantDuration - 0.1f);
	TestTrue("Not yet", Bomb->GetBombState() == EShooterBombState::Carried);
	TickSeconds(World, 0.2f);
	TestTrue("Planted",
		Bomb->GetBombState() == EShooterBombState::Planted && State->GetBombState() == EShooterBombState::Planted);
	TestTrue("At A", State->GetBombSite() == FName(TEXT("A")));
	TestNull("No longer carried", T->GetCarriedBomb());
	TestEqual("The plant reward", TState->GetMoney(), TMoney + 300);

	AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	TestFalse("Too far to defuse", CT->StartUse());
	CT->Reset(Bomb->GetActorLocation() - FVector(60.0f, 0.0f, 0.0f));
	TickFrames(World, 1);
	TestTrue("Defusing", CT->StartUse());
	TestEqual("Without a kit: 10 s", Bomb->GetDefuseEndTime() - World.GetTimeSeconds(), 10.0f, 0.05f);
	TickSeconds(World, 2.0f);
	CT->Reset(Bomb->GetActorLocation() - FVector(600.0f, 0.0f, 0.0f));
	TickFrames(World, 1);
	TestFalse("Walked off: stopped", CT->IsDefusing());
	TestNull("The bomb is free again", Bomb->GetDefuser());

	CT->SetDefuseKit(true);
	CT->Reset(Bomb->GetActorLocation() - FVector(60.0f, 0.0f, 0.0f));
	TickFrames(World, 1);
	TestTrue("Defusing with a kit", CT->StartUse());
	TestEqual("With a kit: 5 s", Bomb->GetDefuseEndTime() - World.GetTimeSeconds(), 5.0f, 0.05f);
	const int32 TBefore = TState->GetMoney();
	TickSeconds(World, 5.0f);
	TestTrue("Defused", Bomb->GetBombState() == EShooterBombState::Defused);
	TestTrue("CT win", State->GetLastRoundEndReason() == EShooterRoundEndReason::BombDefused);
	TestEqual("The losing T's bonus with the plant", TState->GetMoney(), TBefore + 1400 + 800);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBombSurvivingCarrierTest, "ShooterGame.Bomb.SurvivingCarrierLetsGo",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBombSurvivingCarrierTest::RunTest(const FString& Parameters)
{
	// A carrier alive at the round's end loses the round's bomb with the clean-up: in the next round exactly one
	// terrorist carries, and it is the new round's bomb.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 3);
	TickUntilLive(World, *GameMode);
	GameMode->EndRound(EShooterRoundEndReason::TargetSaved);
	TickSeconds(World, GameMode->RoundRestartDelay + 0.1f);
	TickUntilLive(World, *GameMode);
	int32 Carriers = 0;
	for (const AShooterCharacter* T : GetAlive(World, EShooterTeam::T))
	{
		if (T->GetCarriedBomb() != nullptr)
		{
			++Carriers;
			TestTrue("The round's bomb", T->GetCarriedBomb() == GameMode->GetBomb());
			TestFalse("Not a destroyed one", T->GetCarriedBomb()->IsPendingKillPending());
		}
	}
	TestEqual("One carrier", Carriers, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBombNoPlantAfterTheRoundTest, "ShooterGame.Bomb.NoPlantAfterTheRound",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBombNoPlantAfterTheRoundTest::RunTest(const FString& Parameters)
{
	// A plant under way when the round ends stops, pays nothing and leaves the bomb unplanted; a new one is refused
	// while the result shows.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	GameMode->RoundRestartDelay = 5.0f;
	TickUntilLive(World, *GameMode);
	AShooterBomb* Bomb = GameMode->GetBomb();
	AShooterCharacter* T = Bomb != nullptr ? Bomb->GetCarrier() : nullptr;
	if (!TestNotNull("A carrier", T))
	{
		return false;
	}
	T->Reset(FVector(100.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	TickFrames(World, 2);
	AShooterPlayerState* TState = GetTeamStates(*GameMode, EShooterTeam::T)[0];
	TestTrue("Planting", T->StartUse());
	TickSeconds(World, 1.0f);
	const int32 Money = TState->GetMoney();
	GameMode->EndRound(EShooterRoundEndReason::CTsEliminated);
	const int32 MoneyAfterWin = TState->GetMoney();
	TickSeconds(World, 3.0f);
	TestFalse("Not planted", Bomb->GetBombState() == EShooterBombState::Planted);
	TestFalse("The state neither", GameMode->GetShooterGameState()->GetBombState() == EShooterBombState::Planted);
	TestEqual("No plant reward", TState->GetMoney(), MoneyAfterWin);
	TestTrue("The win paid", MoneyAfterWin > Money);
	TestFalse("No new plant", T->StartUse());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBombExplosionTest, "ShooterGame.Bomb.Explosion",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBombExplosionTest::RunTest(const FString& Parameters)
{
	// The planted bomb explodes after BombTimer: a pawn next to it dies, one far away lives, through no wall test.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 2, 1);
	AShooterGameState* State = GameMode->GetShooterGameState();
	TickUntilLive(World, *GameMode);
	if (!TestNotNull("Planted", PlantInSiteA(World, *GameMode)))
	{
		return false;
	}
	AShooterBomb* Bomb = GameMode->GetBomb();
	TArray<AShooterCharacter*> CTs = GetAlive(World, EShooterTeam::CT);
	CTs[0]->Reset(Bomb->GetActorLocation() + FVector(-300.0f, 0.0f, 0.0f));
	CTs[1]->Reset(Bomb->GetActorLocation() + FVector(-3000.0f, 0.0f, 0.0f));
	TestTrue("The explosion time", FMath::IsNearlyEqual(State->GetBombExplodeTime(), Bomb->GetExplodeTime()));
	TickSeconds(World, Bomb->BombTimer);
	TestTrue("Exploded", Bomb->GetBombState() == EShooterBombState::Exploded);
	TestFalse("Near: dead", CTs[0]->IsAlive());
	TestTrue("Far: alive", CTs[1]->IsAlive());
	TestTrue("T win", State->GetLastRoundEndReason() == EShooterRoundEndReason::TargetBombed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBuyRulesTest, "ShooterGame.Buy.Rules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBuyRulesTest::RunTest(const FString& Parameters)
{
	// Buying: in the team's buy zone, within the buy time, with the money; the same weapon twice is refused, another
	// in its slot is dropped; kevlar, kevlar and helmet (the helmet alone after kevlar), the defuse kit for the CT.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	TickFrames(World, 1);
	AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	AShooterCharacter* T = GetAlive(World, EShooterTeam::T)[0];
	AShooterPlayerState* CTState = CT->GetController()->GetPlayerState<AShooterPlayerState>();
	FString Reason;
	TestFalse("An AK-47 with 800", GameMode->Buy(CT, TEXT("ak47"), &Reason));
	TestTrue("Not enough money", Reason.Contains(TEXT("money")));
	TestTrue("A vest", GameMode->Buy(CT, TEXT("vest")));
	TestEqual("150 left", CTState->GetMoney(), 150);
	TestTrue("Full kevlar", CT->GetArmor() == 100.0f && !CT->HasHelmet());
	TestEqual("The helmet alone costs 350", GameMode->GetPrice(*CT, TEXT("vesthelm")), 350);
	TestFalse("The same vest again", GameMode->Buy(CT, TEXT("vest")));
	TestEqual("No kit for a T", GameMode->GetPrice(*T, TEXT("defuser")), -1);
	TestEqual("The kit for a CT", GameMode->GetPrice(*CT, TEXT("defuser")), 200);

	CTState->SetMoney(9000, GameMode->MaxMoney);
	TestTrue("An AK-47", GameMode->Buy(CT, TEXT("ak47")));
	TestTrue("Drawn", CT->GetWeapon() != nullptr && CT->GetWeapon()->WeaponName == TEXT("ak47"));
	TestFalse("A second one", GameMode->Buy(CT, TEXT("ak47")));
	AShooterWeapon* Rifle = CT->GetWeapon();
	TestTrue("An AWP", GameMode->Buy(CT, TEXT("awp")));
	TestTrue("The AK-47 was dropped", Rifle->IsDropped());
	TestEqual("9000 - 2500 - 4750", CTState->GetMoney(), 1750);
	TestTrue("A grenade", GameMode->Buy(CT, TEXT("hegrenade")));
	TestFalse("A second grenade", GameMode->Buy(CT, TEXT("hegrenade")));
	TestTrue("A kit", GameMode->Buy(CT, TEXT("defuser")) && CT->HasDefuseKit());
	TestFalse("Nothing called so", GameMode->Buy(CT, TEXT("bazooka")));

	// Outside the zone, then after the buy time.
	CT->Reset(FVector(0.0f, 0.0f, 0.0f));
	TickFrames(World, 1);
	TestFalse("Outside the buy zone", GameMode->Buy(CT, TEXT("vesthelm"), &Reason));
	TestTrue("The reason", Reason.Contains(TEXT("buy zone")));
	TestTrue("The enemy's zone is not ours",
		GameMode->FindZone(World, FVector(1500.0f, 0.0f, 0.0f), AShooterGameMode::BuyZoneTag, FName(TEXT("CT"))) ==
			nullptr);
	CT->Reset(FVector(-1500.0f, 0.0f, 0.0f));
	TickSeconds(World, GameMode->BuyTime);
	TestFalse("After the buy time", GameMode->Buy(CT, TEXT("vesthelm"), &Reason));
	TestTrue("The reason", Reason.Contains(TEXT("buy time")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRoundsDeterministicTest, "ShooterGame.Rounds.DeterministicBomb",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRoundsDeterministicTest::RunTest(const FString& Parameters)
{
	// The bomb's carrier comes from the round stream: the same seed gives the same carriers round after round.
	TArray<FString> Carriers[3];
	const int32 Seeds[3] = {7, 7, 12345};
	for (int32 Run = 0; Run < 3; ++Run)
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		AShooterGameMode* GameMode = SetUpMatch(World, 1, 5);
		GameMode->RandomSeed = Seeds[Run];
		for (int32 Round = 0; Round < 6; ++Round)
		{
			TickUntilLive(World, *GameMode);
			const AShooterCharacter* Carrier = GameMode->GetBomb()->GetCarrier();
			Carriers[Run].Add(Carrier->GetController()->GetPlayerState<AShooterPlayerState>()->GetPlayerName());
			GameMode->EndRound(EShooterRoundEndReason::TargetSaved);
			TickSeconds(World, GameMode->RoundRestartDelay);
		}
	}
	TestTrue("The same seed, the same carriers", Carriers[0] == Carriers[1]);
	TestTrue("Another seed, other carriers", Carriers[0] != Carriers[2]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameMatchEndAndRestartTest, "ShooterGame.Rounds.MatchEndAndRestart",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameMatchEndAndRestartTest::RunTest(const FString& Parameters)
{
	// Three rounds a match: the CT win two and the match ends (the pawns freeze); mp_restartgame starts a new one with
	// the scores, the money and the rounds back to the start; a bot added during a live round waits for the next.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	GameMode->MaxRounds = 3;
	AShooterGameState* State = GameMode->GetShooterGameState();
	for (int32 Round = 0; Round < 2; ++Round)
	{
		TickUntilLive(World, *GameMode);
		GameMode->EndRound(EShooterRoundEndReason::TargetSaved);
		TickSeconds(World, GameMode->RoundRestartDelay);
	}
	TestTrue("Match over", State->GetRoundState() == EShooterRoundState::MatchEnd && GameMode->HasMatchEnded());
	TestTrue("The CT won it", State->GetMatchWinner() == EShooterTeam::CT);
	TestTrue("Frozen at the end", GetAlive(World, EShooterTeam::CT)[0]->IsFrozen());

	TestTrue("mp_restartgame", GameMode->ProcessConsoleExec(TEXT("mp_restartgame 0"), *GLog, nullptr));
	TickFrames(World, 1);
	TestTrue("In progress again", GameMode->IsMatchInProgress());
	TestEqual("Round 1", State->GetRoundNumber(), 1);
	TestEqual("CT 0", State->GetTeamScore(EShooterTeam::CT), 0);
	TestEqual("Money back to 800", GetTeamStates(*GameMode, EShooterTeam::T)[0]->GetMoney(), 800);

	TickUntilLive(World, *GameMode);
	TestEqual("A late bot joins", GameMode->AddBots(EShooterTeam::T, 1), 1);
	TestEqual("It waits", GetAlive(World, EShooterTeam::T).Num(), 1);
	GameMode->EndRound(EShooterRoundEndReason::TargetSaved);
	TickSeconds(World, GameMode->RoundRestartDelay);
	TestEqual("It plays the next round", GetAlive(World, EShooterTeam::T).Num(), 2);
	TestTrue("bot_kick all", GameMode->ProcessConsoleExec(TEXT("bot_kick all"), *GLog, nullptr));
	TestEqual("Nobody left", GameMode->GetTeamSize(EShooterTeam::T) + GameMode->GetTeamSize(EShooterTeam::CT), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRestartInTheFirstRoundTest, "ShooterGame.Rounds.RestartInTheFirstRound",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRestartInTheFirstRoundTest::RunTest(const FString& Parameters)
{
	// mp_restartgame while the first round's result shows starts round 1 again: the bots buy again in it, and the
	// match checker starts the score over (no false violation in the rounds after).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	FShooterMatchChecker Checker;
	TickUntilLive(World, *GameMode);
	AShooterCharacter* T = GetAlive(World, EShooterTeam::T)[0];
	AShooterAIController* Bot = Cast<AShooterAIController>(T->GetController());
	if (!TestNotNull("A bot", Bot))
	{
		return false;
	}
	TestTrue("It buys in round 1", Bot->BuyForRound().Num() > 0);
	GameMode->EndRound(EShooterRoundEndReason::TargetSaved);
	Checker.Tick(*GameMode);
	GameMode->RestartGame(0.0f);
	for (int32 Frame = 0; Frame < 3; ++Frame)
	{
		World.Tick(FrameTime);
		Checker.Tick(*GameMode);
	}
	TickUntilLive(World, *GameMode);
	const AShooterGameState* State = GameMode->GetShooterGameState();
	TestEqual("Round 1 again", State->GetRoundNumber(), 1);
	TestTrue("It buys in the new round 1", Bot->BuyForRound().Num() > 0);
	GameMode->EndRound(EShooterRoundEndReason::CTsEliminated);
	Checker.Tick(*GameMode);
	TickSeconds(World, GameMode->RoundRestartDelay + 0.1f);
	TickUntilLive(World, *GameMode);
	GameMode->EndRound(EShooterRoundEndReason::TargetSaved);
	Checker.Tick(*GameMode);
	TestEqual("Three rounds ended", Checker.GetRoundsPlayed(), 3);
	for (const FString& Violation : Checker.GetViolations())
	{
		AddError(Violation);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameKillCreditOfAProjectileTest, "ShooterGame.Economy.ProjectileKillCredit",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameKillCreditOfAProjectileTest::RunTest(const FString& Parameters)
{
	// A grenade's kill is credited from the projectile (the throwing weapon is gone by then): its name in the feed, its
	// reward to the thrower.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	AShooterCharacter* T = GetAlive(World, EShooterTeam::T)[0];
	AShooterProjectile* Grenade =
		World.SpawnActor<AShooterProjectile>(FVector(0.0f, 0.0f, 500.0f), FRotator::ZeroRotator);
	Grenade->WeaponName = TEXT("hegrenade");
	Grenade->KillReward = 300;
	AShooterPlayerState* CTState = GetTeamStates(*GameMode, EShooterTeam::CT)[0];
	CTState->SetMoney(1000, GameMode->MaxMoney);
	(void)UGameplayStatics::ApplyDamage(T, 500.0f, CT->GetController(), Grenade, UDamageType::StaticClass());
	TestFalse("Killed", T->IsAlive());
	TestEqual("The reward", CTState->GetMoney(), 1300);
	const TArray<FShooterKillFeedEntry>& Feed = GameMode->GetShooterGameState()->GetKillFeed();
	TestTrue("The feed names the grenade", Feed.Num() > 0 && Feed.Last().WeaponName == TEXT("hegrenade"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameHUDRoundInfoTest, "ShooterGame.HUD.RoundInfo",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameHUDRoundInfoTest::RunTest(const FString& Parameters)
{
	// In a match the HUD draws more than the crosshair (the clock, the score, the round), and the crosshair opens with
	// the spread.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	(void)SetUpMatch(World, 1, 1);
	TickFrames(World, 1);
	AShooterHUD* HUD = World.SpawnActor<AShooterHUD>();
	FCanvas Canvas(1280, 720);
	HUD->Paint(Canvas);
	TArray<FCanvasVertex> Vertices;
	Canvas.GetTriangles(Vertices);
	TestTrue("The round's text too", Vertices.Num() > 4 * 6);
	TestEqual("No pawn: the base gap", HUD->GetCrosshairGap(), HUD->CrosshairGap);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
