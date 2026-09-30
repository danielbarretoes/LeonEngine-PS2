#include "Camera/PlayerCameraManager.h"
#include "CanvasTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawn.h"
#include "HAL/UnrealMemory.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "ShooterAIController.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterHUD.h"
#include "ShooterMatchChecker.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"
#include "Weapons/ShooterProjectile.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterWeapon_AWP.h"
#include "Weapons/ShooterWeapon_Instant.h"
#include "Weapons/ShooterWeapon_Projectile.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRoundsDeadPlayerPlaysAgainTest, "ShooterGame.Rounds.DeadPlayerPlaysAgain",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRoundsDeadPlayerPlaysAgainTest::RunTest(const FString& Parameters)
{
	// A player who dies spectates for the rest of the round; the next round gives it a new shooter on its team's side,
	// and it plays again (not the spectator it flew).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 2);
	AShooterGameState* State = GameMode->GetShooterGameState();
	AShooterPlayerController* Player = World.SpawnActor<AShooterPlayerController>();
	AShooterPlayerState* PlayerState = Player->GetPlayerState<AShooterPlayerState>();
	if (!TestNotNull("The game's state", State) || !TestNotNull("The player's state", PlayerState))
	{
		return false;
	}
	PlayerState->SetTeam(EShooterTeam::CT);
	GameMode->PostLogin(Player);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* FirstPawn = Cast<AShooterCharacter>(Player->GetPawn());
	if (!TestNotNull("The player plays", FirstPawn))
	{
		return false;
	}

	Player->SetBuyMenuOpen(true);
	FirstPawn->Suicide();
	TickFrames(World, 1);
	TestTrue("Spectating", Player->IsInState(NAME_Spectating));
	TestFalse("Death closes the buy menu", Player->IsBuyMenuOpen());
	TestTrue("The round goes on (a CT bot lives)", State->GetRoundState() == EShooterRoundState::Live);
	KillTeam(World, EShooterTeam::T);
	TickFrames(World, 1);
	TestTrue("Round over", State->GetRoundState() == EShooterRoundState::RoundEnd);
	TickSeconds(World, GameMode->RoundRestartDelay);
	TestEqual("Round 2", State->GetRoundNumber(), 2);

	const AShooterCharacter* NewPawn = Cast<AShooterCharacter>(Player->GetPawn());
	if (!TestNotNull("A shooter again", NewPawn))
	{
		return false;
	}
	TestTrue("A new one", NewPawn != FirstPawn && NewPawn->IsAlive());
	TestTrue("Playing", Player->IsInState(NAME_Playing));
	TestNull("No spectator", Player->GetSpectatorPawn());
	TestEqual("On its side", NewPawn->GetActorLocation().X, -1500.0f, 1.0f);
	TestTrue("With the pistol", NewPawn->GetWeaponInSlot(EShooterWeaponSlot::Secondary) != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRoundsSpectateTeammatesTest, "ShooterGame.Rounds.SpectateTeammates",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRoundsSpectateTeammatesTest::RunTest(const FString& Parameters)
{
	// A dead player watches its living teammates through their eyes (CS: spec_next; it ends the death cam), in the game
	// state's order and wrapping, never an enemy; when the watched one dies the camera moves on to the next; at the
	// next round its camera is its own pawn's again.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 2, 2);
	AShooterPlayerController* Player = World.SpawnActor<AShooterPlayerController>();
	// A local player: its controller gets the camera the spectator views through.
	Player->SetPlayer(NewObject<ULocalPlayer>(Player));
	AShooterPlayerState* PlayerState = Player->GetPlayerState<AShooterPlayerState>();
	if (!TestNotNull("The player's state", PlayerState))
	{
		return false;
	}
	PlayerState->SetTeam(EShooterTeam::CT);
	GameMode->PostLogin(Player);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* FirstPawn = Cast<AShooterCharacter>(Player->GetPawn());
	if (!TestNotNull("The player plays", FirstPawn) || !TestNotNull("A camera", Player->PlayerCameraManager))
	{
		return false;
	}
	Player->ViewNextPlayer();
	TestNull("Playing, the player watches nobody else", Player->GetViewedPlayer());

	FirstPawn->Suicide();
	TickFrames(World, 1);
	Player->ViewNextPlayer();
	AShooterCharacter* First = Player->GetViewedPlayer();
	if (!TestNotNull("A teammate watched", First))
	{
		return false;
	}
	TestTrue("A living CT", First->IsAlive() && First->GetTeam() == EShooterTeam::CT);
	Player->ViewNextPlayer();
	AShooterCharacter* Second = Player->GetViewedPlayer();
	TestTrue("The other CT", Second != nullptr && Second != First && Second->GetTeam() == EShooterTeam::CT);
	Player->ViewNextPlayer();
	TestTrue("Wraps to the first", Player->GetViewedPlayer() == First);
	First->Suicide();
	TickFrames(World, 1);
	TestTrue("The watched teammate died: the one left", Player->GetViewedPlayer() == Second);
	Player->ViewNextPlayer();
	TestTrue("Only the one left", Player->GetViewedPlayer() == Second);

	KillTeam(World, EShooterTeam::T);
	TickFrames(World, 1);
	TickSeconds(World, GameMode->RoundRestartDelay);
	const APawn* NewPawn = Player->GetPawn();
	TestTrue("Playing again", NewPawn != nullptr && Player->IsInState(NAME_Playing));
	TestTrue("Through its own eyes", Player->PlayerCameraManager->GetViewTarget() == NewPawn);
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
	AShooterWeapon* Awp = CT->GiveWeapon(AShooterWeapon_AWP::StaticClass());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBombCarrierKilledInTheAirTest, "ShooterGame.Bomb.CarrierKilledInTheAir",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBombCarrierKilledInTheAirTest::RunTest(const FString& Parameters)
{
	// The carrier dies falling, 4 m up: the body and the bomb land on the floor, where a terrorist picks the bomb up.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 2);
	TickUntilLive(World, *GameMode);
	AShooterBomb* Bomb = GameMode->GetBomb();
	AShooterCharacter* Carrier = Bomb != nullptr ? Bomb->GetCarrier() : nullptr;
	if (!TestNotNull("A carrier", Carrier))
	{
		return false;
	}
	Carrier->Reset(FVector(0.0f, 1000.0f, 400.0f), FRotator::ZeroRotator);
	TickFrames(World, 2);
	TestTrue("Falling", Carrier->IsFalling());
	Carrier->Suicide();
	TestTrue("Dropped", Bomb->GetBombState() == EShooterBombState::Dropped);
	TestEqual("The bomb on the floor", Bomb->GetActorLocation().Z, 0.0f, 1.0f);
	TestEqual("The body on the floor", Carrier->GetActorLocation().Z, 0.0f, 1.0f);

	AShooterCharacter* Mate = nullptr;
	for (AShooterCharacter* T : GetAlive(World, EShooterTeam::T))
	{
		Mate = T;
	}
	if (!TestNotNull("A terrorist left", Mate))
	{
		return false;
	}
	Mate->Reset(Bomb->GetActorLocation() + FVector(20.0f, 0.0f, 0.0f));
	TickFrames(World, 2);
	TestTrue("Picked up", Bomb->GetCarrier() == Mate);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRoundsFrozenTriggerTest, "ShooterGame.Rounds.FrozenTriggerDoesNotFire",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRoundsFrozenTriggerTest::RunTest(const FString& Parameters)
{
	// A trigger pulled on the weapon itself during the freeze (or held into the match's end) fires nothing while the
	// pawn is frozen; the round going live lets it fire.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	GameMode->FreezeTime = 3.0f;
	TickSeconds(World, 1.5f);
	AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	AShooterWeapon* Pistol = CT->GetWeapon();
	if (!TestNotNull("A pistol", Pistol) || !TestTrue("Frozen", CT->IsFrozen()))
	{
		return false;
	}
	Pistol->StartFire();
	TickFrames(World, 10);
	TestEqual("No shot in the freeze", Pistol->GetShotsFired(), 0);
	TickUntilLive(World, *GameMode);
	TickFrames(World, 1);
	TestEqual("It fires once live", Pistol->GetShotsFired(), 1);
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
	// Past the explosion's 4445 cm (4667 cm, on the floor's diagonal).
	CTs[1]->Reset(Bomb->GetActorLocation() + FVector(3300.0f, -3300.0f, 0.0f));
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
	TestFalse("An M4A1 with 800", GameMode->Buy(CT, TEXT("m4a1"), &Reason));
	TestTrue("Not enough money", Reason.Contains(TEXT("money")));
	TestTrue("A vest", GameMode->Buy(CT, TEXT("vest")));
	TestEqual("150 left", CTState->GetMoney(), 150);
	TestTrue("Full kevlar", CT->GetArmor() == 100.0f && !CT->HasHelmet());
	TestEqual("The helmet alone costs 350", GameMode->GetPrice(*CT, TEXT("vesthelm")), 350);
	TestFalse("The same vest again", GameMode->Buy(CT, TEXT("vest")));
	TestEqual("No kit for a T", GameMode->GetPrice(*T, TEXT("defuser")), -1);
	TestEqual("The kit for a CT", GameMode->GetPrice(*CT, TEXT("defuser")), 200);

	CTState->SetMoney(9000, GameMode->MaxMoney);
	TestTrue("An M4A1", GameMode->Buy(CT, TEXT("m4a1")));
	TestTrue("Drawn", CT->GetWeapon() != nullptr && CT->GetWeapon()->WeaponName == TEXT("m4a1"));
	TestFalse("A second one", GameMode->Buy(CT, TEXT("m4a1")));
	AShooterWeapon* Rifle = CT->GetWeapon();
	TestTrue("An AWP", GameMode->Buy(CT, TEXT("awp")));
	TestTrue("The M4A1 was dropped", Rifle->IsDropped());
	TestEqual("9000 - 3100 - 4750", CTState->GetMoney(), 1150);
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
		GameMode->FindZone(FVector(1500.0f, 0.0f, 0.0f), AShooterGameMode::BuyZoneTag, FName(TEXT("CT"))) == nullptr);
	CT->Reset(FVector(-1500.0f, 0.0f, 0.0f));
	// The buy time counts from the freeze's end.
	TickSeconds(World, GameMode->FreezeTime + GameMode->BuyTime);
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
	// The same team wins both rounds on the same side (the halftime has its own tests).
	GameMode->bHalftime = false;
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
	// In a match the HUD draws more than the crosshair (the clock, the score, the round: a textured sprite a glyph of
	// the engine's small font), and the crosshair opens with the spread.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	(void)SetUpMatch(World, 1, 1);
	TickFrames(World, 1);
	AShooterHUD* HUD = World.SpawnActor<AShooterHUD>();
	FCanvas Canvas(1280, 720);
	HUD->Paint(Canvas);
	TArray<FCanvasVertex> Vertices;
	TArray<FCanvasPrimitiveRun> Runs;
	Canvas.GetPrimitives(Vertices, Runs);
	TestTrue("The round's text too", Vertices.Num() > 4 * 2);
	const UFont* Font = UEngine::GetSmallFont();
	int32 Glyphs = 0;
	bool bSmallFont = Font != nullptr;
	for (const FCanvasPrimitiveRun& Run : Runs)
	{
		if (Run.Texture != nullptr)
		{
			Glyphs += Run.NumVertices / 2;
			bSmallFont &= Run.Type == ECanvasPrimitive::Rectangle && Font->Textures.Contains(Run.Texture);
		}
	}
	// "CT 0", "0:.." and "0 T" and "Round 1": more than a dozen glyphs, each one sprite of the font's page.
	TestTrue("A sprite a glyph, in the small font", Glyphs > 12 && bSmallFont);
	TestEqual("No pawn: the base gap", HUD->GetCrosshairGap(720.0f), HUD->CrosshairGap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBombExplosionRadiusTest, "ShooterGame.Bomb.ExplosionRadius",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBombExplosionRadiusTest::RunTest(const FString& Parameters)
{
	// ps2-shipping N6: CS 1.6's C4 reaches 1750 units, 4445 cm (the config held the units as centimetres): a pawn 40 m
	// from the bomb is hurt, one past 44.45 m is not.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 2, 1);
	TickUntilLive(World, *GameMode);
	if (!TestNotNull("Planted", PlantInSiteA(World, *GameMode)))
	{
		return false;
	}
	AShooterBomb* Bomb = GameMode->GetBomb();
	TestEqual("The radius, cm", Bomb->ExplosionRadius, 4445.0f);
	TArray<AShooterCharacter*> CTs = GetAlive(World, EShooterTeam::CT);
	const FVector BombLocation = Bomb->GetActorLocation();
	CTs[0]->Reset(BombLocation + FVector(-4000.0f, 0.0f, 0.0f));
	// 4667 cm away, on the floor's diagonal.
	CTs[1]->Reset(BombLocation + FVector(3300.0f, -3300.0f, 0.0f));
	TickFrames(World, 1);
	Bomb->Explode();
	TestTrue("40 m: hurt", CTs[0]->GetHealth() < 100.0f);
	TestTrue("40 m: alive", CTs[0]->IsAlive());
	TestEqual("Past 44.45 m: untouched", CTs[1]->GetHealth(), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBombExplosionRespectsArmorTest, "ShooterGame.Bomb.ExplosionRespectsArmor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBombExplosionRespectsArmorTest::RunTest(const FString& Parameters)
{
	// ps2-shipping N6: CS 1.6 armors a blast like the rest (the HE grenade's rule): two CT at the same distance, the
	// one in kevlar takes half the health damage of the other and its armor half the rest.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 2, 1);
	TickUntilLive(World, *GameMode);
	if (!TestNotNull("Planted", PlantInSiteA(World, *GameMode)))
	{
		return false;
	}
	AShooterBomb* Bomb = GameMode->GetBomb();
	TArray<AShooterCharacter*> CTs = GetAlive(World, EShooterTeam::CT);
	const FVector BombLocation = Bomb->GetActorLocation();
	AShooterCharacter* Armored = CTs[0];
	AShooterCharacter* Bare = CTs[1];
	Armored->Reset(BombLocation + FVector(0.0f, 3800.0f, 0.0f));
	Bare->Reset(BombLocation + FVector(0.0f, -3800.0f, 0.0f));
	Armored->SetArmor(100.0f, true);
	TickFrames(World, 1);
	Bomb->Explode();
	const float BareDamage = 100.0f - Bare->GetHealth();
	const float ArmoredDamage = 100.0f - Armored->GetHealth();
	TestTrue("The bare CT is hurt and lives", BareDamage > 0.0f && Bare->IsAlive());
	TestEqual("Kevlar halves the health's damage", ArmoredDamage, BareDamage * 0.5f, 0.01f);
	TestEqual("The armor takes half the rest", Armored->GetArmor(), 100.0f - (BareDamage * 0.25f), 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBuyTimeAfterTheFreezeTest, "ShooterGame.Buy.BuyTimeAfterTheFreeze",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBuyTimeAfterTheFreezeTest::RunTest(const FString& Parameters)
{
	// ps2-shipping N6: CS's mp_buytime counts from the freeze's end (it counted from the freeze's start, so the live
	// round had BuyTime - FreezeTime to buy): buying goes on BuyTime into the live round and stops after.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	GameMode->FreezeTime = 2.0f;
	TickUntilLive(World, *GameMode);
	const AShooterGameState* State = GameMode->GetShooterGameState();
	const float LiveStart = World.GetTimeSeconds();
	TestEqual("It ends BuyTime after the freeze", State->GetBuyEndTime(), LiveStart + GameMode->BuyTime, 0.05f);
	const AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	TickSeconds(World, GameMode->BuyTime - 0.5f);
	FString Reason;
	TestTrue("Still buying BuyTime - 0.5 s into the round", GameMode->CanBuy(*CT, &Reason));
	TickSeconds(World, 1.0f);
	TestFalse("Over past BuyTime", GameMode->CanBuy(*CT, &Reason));
	TestTrue("The reason", Reason.Contains(TEXT("buy time")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBuyMenuFollowsTheRulesTest, "ShooterGame.Buy.MenuFollowsTheRules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBuyMenuFollowsTheRulesTest::RunTest(const FString& Parameters)
{
	// ps2-shipping N6: the buy menu opens only when its player may buy (in the buy zone, within the buy time, alive and
	// playing) and closes by itself when that stops (CS: the buy time's end closes it); a spectator cannot open it (its
	// keys took the spectator's Cross). A refusal leaves its reason in the last buy's message for the HUD.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	AShooterPlayerController* Player = World.SpawnActor<AShooterPlayerController>();
	AShooterPlayerState* PlayerState = Player->GetPlayerState<AShooterPlayerState>();
	if (!TestNotNull("The player's state", PlayerState))
	{
		return false;
	}
	PlayerState->SetTeam(EShooterTeam::CT);
	GameMode->PostLogin(Player);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* Pawn = Cast<AShooterCharacter>(Player->GetPawn());
	if (!TestNotNull("The player plays", Pawn))
	{
		return false;
	}

	Player->BuyMenu();
	TestTrue("Opens in the buy zone within the buy time", Player->IsBuyMenuOpen());
	TickSeconds(World, GameMode->BuyTime + 0.1f);
	TestFalse("The buy time's end closes it", Player->IsBuyMenuOpen());
	TestTrue("It says why", Player->GetLastBuyMessage().Contains(TEXT("buy time")));
	TestTrue("For the HUD", Player->GetBuyRefusalTime() >= 0.0f);
	Player->BuyMenu();
	TestFalse("Refused after the buy time", Player->IsBuyMenuOpen());

	// The next round: out of the buy zone it is refused; opened in it, walking out closes it.
	KillTeam(World, EShooterTeam::T);
	TickFrames(World, 1);
	TickSeconds(World, GameMode->RoundRestartDelay);
	TestTrue("A new round's freeze", GameMode->GetShooterGameState()->IsFreezeTime());
	const FVector Start = Pawn->GetActorLocation();
	Pawn->Reset(FVector::ZeroVector);
	TickFrames(World, 1);
	Player->BuyMenu();
	TestFalse("Refused out of the buy zone", Player->IsBuyMenuOpen());
	TestTrue("It says why", Player->GetLastBuyMessage().Contains(TEXT("buy zone")));
	Pawn->Reset(Start);
	TickFrames(World, 1);
	Player->BuyMenu();
	TestTrue("Opens back in the zone", Player->IsBuyMenuOpen());
	Pawn->Reset(FVector::ZeroVector);
	TickFrames(World, 1);
	TestFalse("Leaving the zone closes it", Player->IsBuyMenuOpen());

	// Dead: the player spectates, and B (or Start) leaves its Cross alone.
	Pawn->Suicide();
	TickFrames(World, 1);
	TestTrue("Spectating", Player->IsInState(NAME_Spectating));
	Player->BuyMenu();
	TestFalse("Refused to a spectator", Player->IsBuyMenuOpen());
	TestTrue("It says why", Player->GetLastBuyMessage().Contains(TEXT("spectating")));
	return true;
}

// ps2-shipping N30d: the halftime, the death cam and spectating, the radar and the damage indicator.

namespace
{

	/** Ticks Seconds of world time frame by frame, the match checker watching every frame. */
	void TickChecked(UWorld& World, const AShooterGameMode& GameMode, FShooterMatchChecker& Checker, float Seconds)
	{
		const int32 Frames = FMath::CeilToInt(Seconds / FrameTime) + 1;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World.Tick(FrameTime);
			Checker.Tick(GameMode);
		}
	}

	/** Plays until the round is live, then ends it for Reason, the checker watching. */
	void PlayRound(
		UWorld& World, AShooterGameMode& GameMode, FShooterMatchChecker& Checker, EShooterRoundEndReason Reason)
	{
		for (int32 Frame = 0;
			Frame < 600 && GameMode.GetShooterGameState()->GetRoundState() != EShooterRoundState::Live; ++Frame)
		{
			World.Tick(FrameTime);
			Checker.Tick(GameMode);
		}
		GameMode.EndRound(Reason);
		Checker.Tick(GameMode);
	}

	/** A local player on Team, logged in (its HUD, its camera): it plays from the next spawn on. */
	AShooterPlayerController* AddLocalPlayer(UWorld& World, AShooterGameMode& GameMode, EShooterTeam Team)
	{
		AShooterPlayerController* Player = World.SpawnActor<AShooterPlayerController>();
		Player->SetPlayer(NewObject<ULocalPlayer>(Player));
		if (AShooterPlayerState* PlayerState = Player->GetPlayerState<AShooterPlayerState>())
		{
			PlayerState->SetTeam(Team);
		}
		GameMode.PostLogin(Player);
		return Player;
	}

	/** Paints the player's HUD into a canvas of the GS's size. */
	AShooterHUD* PaintHUD(const AShooterPlayerController& Player)
	{
		AShooterHUD* HUD = Cast<AShooterHUD>(Player.MyHUD);
		if (HUD != nullptr)
		{
			FCanvas Canvas(640, 448);
			HUD->Paint(Canvas);
		}
		return HUD;
	}

	/** Tap a key: pressed for a frame, released for a frame. */
	void TapKey(UWorld& World, AShooterPlayerController& Player, const FKey& Key)
	{
		(void)Player.InputKey(Key, IE_Pressed, 1.0f, false);
		TickFrames(World, 1);
		(void)Player.InputKey(Key, IE_Released, 0.0f, false);
		TickFrames(World, 1);
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRoundsHalftimeTest, "ShooterGame.Rounds.HalftimeSwitchesSides",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRoundsHalftimeTest::RunTest(const FString& Parameters)
{
	// Four rounds a match: after round 2 the teams switch sides. The CT win both rounds; at the halftime every player
	// (the bots too) moves to the other team, the scores go with them (CT 0 - T 2), the money starts over and the loss
	// streaks with it, and the second half starts as the first: everyone on the new side's starts with the pistol only,
	// the bomb with one of the new terrorists. FShooterMatchChecker sees the halftime and finds nothing wrong.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 2, 2);
	GameMode->MaxRounds = 4;
	TestEqual("The halftime after round 2", GameMode->GetHalftimeRound(), 2);
	AShooterGameState* State = GameMode->GetShooterGameState();
	FShooterMatchChecker Checker;
	PlayRound(World, *GameMode, Checker, EShooterRoundEndReason::TargetSaved);
	TickChecked(World, *GameMode, Checker, GameMode->RoundRestartDelay);
	const TArray<AShooterPlayerState*> FirstHalfCT = GetTeamStates(*GameMode, EShooterTeam::CT);
	const TArray<AShooterPlayerState*> FirstHalfT = GetTeamStates(*GameMode, EShooterTeam::T);
	AShooterCharacter* RifleCT = GetAlive(World, EShooterTeam::CT)[0];
	(void)RifleCT->GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("ak47")));
	PlayRound(World, *GameMode, Checker, EShooterRoundEndReason::TargetSaved);
	TestEqual("CT 2 before the halftime", State->GetTeamScore(EShooterTeam::CT), 2);
	TestFalse("The first half", State->IsSecondHalf());
	TestEqual("The CT's money: 800 + 2 x 3250", FirstHalfCT[0]->GetMoney(), 7300);
	TestEqual("The T lost twice", GameMode->GetLossStreak(EShooterTeam::T), 2);

	TickChecked(World, *GameMode, Checker, GameMode->RoundRestartDelay);
	TestEqual("Round 3", State->GetRoundNumber(), 3);
	TestTrue("The second half", State->IsSecondHalf() && State->GetHalftimeRound() == 2);
	TestEqual("The scores follow the teams: CT 0", State->GetTeamScore(EShooterTeam::CT), 0);
	TestEqual("T 2", State->GetTeamScore(EShooterTeam::T), 2);
	for (const AShooterPlayerState* PlayerState : FirstHalfCT)
	{
		TestTrue("A first-half CT plays T", PlayerState->GetTeam() == EShooterTeam::T);
		TestEqual("With the start money", PlayerState->GetMoney(), GameMode->StartMoney);
	}
	for (const AShooterPlayerState* PlayerState : FirstHalfT)
	{
		TestTrue("A first-half T plays CT", PlayerState->GetTeam() == EShooterTeam::CT);
		TestEqual("With the start money", PlayerState->GetMoney(), GameMode->StartMoney);
	}
	TestEqual("No loss streak (CT)", GameMode->GetLossStreak(EShooterTeam::CT), 0);
	TestEqual("No loss streak (T)", GameMode->GetLossStreak(EShooterTeam::T), 0);
	TestTrue("The rifle's pawn is gone", RifleCT->IsPendingKillPending());
	const TArray<AShooterCharacter*> NewTerrorists = GetAlive(World, EShooterTeam::T);
	TestEqual("Two terrorists", NewTerrorists.Num(), 2);
	for (const AShooterCharacter* T : NewTerrorists)
	{
		TestEqual("On the T side's starts", T->GetActorLocation().X, 1500.0f, 1.0f);
		TestNull("With the pistol only", T->GetWeaponInSlot(EShooterWeaponSlot::Primary));
		TestTrue("A first-half CT", FirstHalfCT.Contains(T->GetController()->GetPlayerState<AShooterPlayerState>()));
		// The new side's looks (N27): the terrorist's body and arms.
		const USkeletalMesh* Body = T->GetMesh().GetSkeletalMesh();
		const USkeletalMesh* Arms = T->GetMesh1P()->GetSkeletalMesh();
		TestTrue("The terrorist's body and arms",
			Body != nullptr && Body->GetName() == TEXT("SK_Body_T") && Arms != nullptr &&
				Arms->GetName() == TEXT("SK_Arms_T"));
	}
	for (const AShooterCharacter* CT : GetAlive(World, EShooterTeam::CT))
	{
		TestEqual("On the CT side's starts", CT->GetActorLocation().X, -1500.0f, 1.0f);
		const USkeletalMesh* Body = CT->GetMesh().GetSkeletalMesh();
		TestTrue("The counter-terrorist's body", Body != nullptr && Body->GetName() == TEXT("SK_Body_CT"));
	}
	const AShooterBomb* Bomb = GameMode->GetBomb();
	TestTrue("The bomb with a new terrorist",
		Bomb != nullptr && Bomb->GetCarrier() != nullptr && NewTerrorists.Contains(Bomb->GetCarrier()));
	TestEqual("The checker saw the halftime", Checker.GetNumHalftimes(), 1);
	for (const FString& Violation : Checker.GetViolations())
	{
		AddError(Violation);
	}

	// A score the halftime did not swap is flagged.
	FShooterMatchChecker Watcher;
	Watcher.Tick(*GameMode);
	GameMode->RestartGame(0.0f);
	TickChecked(World, *GameMode, Watcher, 0.1f);
	PlayRound(World, *GameMode, Watcher, EShooterRoundEndReason::TargetSaved);
	PlayRound(World, *GameMode, Watcher, EShooterRoundEndReason::TargetSaved);
	TickChecked(World, *GameMode, Watcher, GameMode->RoundRestartDelay);
	TestFalse("A clean halftime", Watcher.HasViolations());
	State->AddTeamScore(EShooterTeam::T);
	State->BeginSecondHalf();
	Watcher.Tick(*GameMode);
	TestTrue("A halftime in the wrong round, nobody moved: flagged", Watcher.HasViolations());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRoundsMatchEndsAtTheMajorityTest,
	"ShooterGame.Rounds.MatchEndsAtTheMajority",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRoundsMatchEndsAtTheMajorityTest::RunTest(const FString& Parameters)
{
	// Four rounds: the team that won the first half 2 - 0 wins round 3 on the other side and has 3 of 4, the majority:
	// the match ends there, won by the side it plays now. Two rounds: one each (the second on the other side), 1 - 1
	// at the last round, a draw.
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
		GameMode->MaxRounds = 4;
		AShooterGameState* State = GameMode->GetShooterGameState();
		FShooterMatchChecker Checker;
		PlayRound(World, *GameMode, Checker, EShooterRoundEndReason::TargetSaved);
		TickChecked(World, *GameMode, Checker, GameMode->RoundRestartDelay);
		const AShooterPlayerState* FirstCT = GetTeamStates(*GameMode, EShooterTeam::CT)[0];
		PlayRound(World, *GameMode, Checker, EShooterRoundEndReason::TargetSaved);
		TickChecked(World, *GameMode, Checker, GameMode->RoundRestartDelay);
		TestTrue("The first CT plays T", FirstCT->GetTeam() == EShooterTeam::T);
		PlayRound(World, *GameMode, Checker, EShooterRoundEndReason::CTsEliminated);
		TestEqual("T 3", State->GetTeamScore(EShooterTeam::T), 3);
		TickChecked(World, *GameMode, Checker, GameMode->RoundRestartDelay);
		TestTrue("Over at the majority",
			GameMode->HasMatchEnded() && State->GetRoundState() == EShooterRoundState::MatchEnd);
		TestEqual("After round 3 of 4", State->GetRoundNumber(), 3);
		TestTrue("Won by the side the team plays now", State->GetMatchWinner() == EShooterTeam::T);
		for (const FString& Violation : Checker.GetViolations())
		{
			AddError(Violation);
		}
	}
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
		GameMode->MaxRounds = 2;
		AShooterGameState* State = GameMode->GetShooterGameState();
		FShooterMatchChecker Checker;
		PlayRound(World, *GameMode, Checker, EShooterRoundEndReason::TargetSaved);
		TickChecked(World, *GameMode, Checker, GameMode->RoundRestartDelay);
		TestTrue("Halftime after round 1", State->IsSecondHalf() && State->GetTeamScore(EShooterTeam::T) == 1);
		PlayRound(World, *GameMode, Checker, EShooterRoundEndReason::TargetSaved);
		TickChecked(World, *GameMode, Checker, GameMode->RoundRestartDelay);
		TestTrue("Over at the last round", GameMode->HasMatchEnded() && State->GetRoundNumber() == 2);
		TestTrue("1 - 1: a draw", State->GetMatchWinner() == EShooterTeam::None);
		for (const FString& Violation : Checker.GetViolations())
		{
			AddError(Violation);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameSpectateDeathCamTest, "ShooterGame.Spectate.DeathCamThenTeammates",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameSpectateDeathCamTest::RunTest(const FString& Parameters)
{
	// Killed, the player looks at its killer from the corpse's eyes (held there) for DeathCamDuration, the HUD naming
	// the killer; then it watches a living teammate through its eyes; with every teammate dead it flies free.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 2, 1);
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::CT);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* Pawn = Cast<AShooterCharacter>(Player->GetPawn());
	AShooterCharacter* Killer = GetAlive(World, EShooterTeam::T)[0];
	if (!TestNotNull("The player plays", Pawn) || !TestNotNull("A camera", Player->PlayerCameraManager))
	{
		return false;
	}
	// The killer off to the side, so the death cam has to turn to it.
	Killer->Reset(FVector(-600.0f, 900.0f, 0.0f), FRotator::ZeroRotator);
	TickFrames(World, 1);
	(void)UGameplayStatics::ApplyDamage(
		Pawn, 500.0f, Killer->GetController(), Killer->GetWeapon(), UDamageType::StaticClass());
	const float DeathTime = World.GetTimeSeconds();
	TestFalse("Dead", Pawn->IsAlive());
	TestTrue("The death cam", Player->GetSpectatorMode() == EShooterSpectatorMode::DeathCam);
	TestTrue("On the killer", Player->GetDeathCamTarget() == Killer);
	TickFrames(World, 1);
	const ASpectatorPawn* Spectator = Player->GetSpectatorPawn();
	if (!TestNotNull("A spectator", Spectator))
	{
		return false;
	}
	TestTrue("Viewed from the spectator", Player->PlayerCameraManager->GetViewTarget() == Spectator);
	TestNull("Nobody else watched", Player->GetViewedPlayer());
	FVector Eyes;
	FVector KillerEyes;
	FRotator Unused;
	Spectator->GetActorEyesViewPoint(Eyes, Unused);
	Killer->GetActorEyesViewPoint(KillerEyes, Unused);
	const FRotator Expected = (KillerEyes - Eyes).Rotation();
	TestEqual("Looks at the killer (yaw)", Player->GetControlRotation().Yaw, Expected.Yaw, 0.5f);
	TestEqual("Looks at the killer (pitch)", Player->GetControlRotation().Pitch, Expected.Pitch, 0.5f);
	TestTrue("From the corpse", FVector::Dist2D(Spectator->GetActorLocation(), Pawn->GetActorLocation()) < 1.0f);
	const AShooterHUD* HUD = PaintHUD(*Player);
	if (!TestNotNull("The player's HUD", HUD))
	{
		return false;
	}
	// The bots' team-neutral names (the third bot made: two CTs, then the terrorist).
	TestEqual("The HUD names the killer", HUD->GetSpectatorText(),
		FString::Printf(TEXT("Killed by %s"), *GameMode->GetBotName(2)));
	TestEqual("CS's names", GameMode->GetBotName(2), FString(TEXT("Bert")));

	// W held: the death cam does not fly.
	const FVector Held = Spectator->GetActorLocation();
	(void)Player->InputKey(EKeys::W, IE_Pressed, 1.0f, false);
	while (World.GetTimeSeconds() - DeathTime < Player->DeathCamDuration - 0.1f)
	{
		TickFrames(World, 1);
	}
	(void)Player->InputKey(EKeys::W, IE_Released, 0.0f, false);
	TestTrue("Still the death cam just before its end", Player->GetSpectatorMode() == EShooterSpectatorMode::DeathCam);
	TestTrue("Held at the corpse", Spectator->GetActorLocation().Equals(Held, 0.01f));
	while (World.GetTimeSeconds() - DeathTime < Player->DeathCamDuration + 0.1f)
	{
		TickFrames(World, 1);
	}
	const AShooterCharacter* Watched = Player->GetViewedPlayer();
	TestTrue("Then a teammate",
		Player->GetSpectatorMode() == EShooterSpectatorMode::Player && Watched != nullptr && Watched->IsAlive() &&
			Watched->GetTeam() == EShooterTeam::CT);
	TestTrue("Through its eyes", Player->PlayerCameraManager->GetViewTarget() == Watched);
	(void)PaintHUD(*Player);
	const AShooterPlayerState* WatchedState =
		Watched != nullptr ? Watched->GetController()->GetPlayerState<AShooterPlayerState>() : nullptr;
	TestTrue("The HUD names it",
		WatchedState != nullptr &&
			HUD->GetSpectatorText().StartsWith(FString::Printf(TEXT("Spectating %s"), *WatchedState->GetPlayerName())));

	KillTeam(World, EShooterTeam::CT);
	TickFrames(World, 1);
	TestTrue("Nobody left: the free look", Player->GetSpectatorMode() == EShooterSpectatorMode::FreeLook);
	TestTrue("Through the spectator", Player->PlayerCameraManager->GetViewTarget() == Player->GetSpectatorPawn());
	(void)PaintHUD(*Player);
	TestEqual("The HUD says so", HUD->GetSpectatorText(), FString(TEXT("Free look")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameSpectateCyclingTest, "ShooterGame.Spectate.CyclingSkipsTheDead",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameSpectateCyclingTest::RunTest(const FString& Parameters)
{
	// A dead player's keys: Fire ends the death cam and watches the next living teammate, Fire again the next (in the
	// game state's order, wrapping), the right button the one before; a dead teammate is skipped both ways; Jump
	// switches to the free look and back.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 3, 1);
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::CT);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* Pawn = Cast<AShooterCharacter>(Player->GetPawn());
	if (!TestNotNull("The player plays", Pawn))
	{
		return false;
	}
	const TArray<AShooterCharacter*> Mates =
		GetAlive(World, EShooterTeam::CT)
			.FilterByPredicate([Pawn](const AShooterCharacter* Mate) { return Mate != Pawn; });
	if (!TestEqual("Three teammates", Mates.Num(), 3))
	{
		return false;
	}
	Pawn->Suicide();
	TickFrames(World, 1);
	TestTrue("The death cam", Player->GetSpectatorMode() == EShooterSpectatorMode::DeathCam);
	TapKey(World, *Player, EKeys::LeftMouseButton);
	TestTrue("Fire ends it: the first teammate", Player->GetViewedPlayer() == Mates[0]);
	TapKey(World, *Player, EKeys::LeftMouseButton);
	TestTrue("The second", Player->GetViewedPlayer() == Mates[1]);
	TapKey(World, *Player, EKeys::LeftMouseButton);
	TestTrue("The third", Player->GetViewedPlayer() == Mates[2]);
	TapKey(World, *Player, EKeys::LeftMouseButton);
	TestTrue("Wraps to the first", Player->GetViewedPlayer() == Mates[0]);
	TapKey(World, *Player, EKeys::RightMouseButton);
	TestTrue("The right button: the one before, wrapping", Player->GetViewedPlayer() == Mates[2]);

	Mates[1]->Suicide();
	TickFrames(World, 1);
	TapKey(World, *Player, EKeys::LeftMouseButton);
	TestTrue("Next skips the dead one", Player->GetViewedPlayer() == Mates[0]);
	TapKey(World, *Player, EKeys::LeftMouseButton);
	TestTrue("Next skips the dead one", Player->GetViewedPlayer() == Mates[2]);
	TapKey(World, *Player, EKeys::RightMouseButton);
	TestTrue("Previous skips it too", Player->GetViewedPlayer() == Mates[0]);
	Player->ViewPrevPlayer();
	TestTrue("ViewPrevPlayer wraps past it", Player->GetViewedPlayer() == Mates[2]);

	TapKey(World, *Player, EKeys::SpaceBar);
	TestTrue("Jump: the free look", Player->GetSpectatorMode() == EShooterSpectatorMode::FreeLook);
	TestNull("Nobody watched", Player->GetViewedPlayer());
	TapKey(World, *Player, EKeys::SpaceBar);
	TestTrue("Jump again: a teammate",
		Player->GetSpectatorMode() == EShooterSpectatorMode::Player && Player->GetViewedPlayer() != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameHUDRadarTest, "ShooterGame.HUD.Radar",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameHUDRadarTest::RunTest(const FString& Parameters)
{
	// The radar's projection: ahead is up and the right is right, it turns with the yaw, and what lies beyond the range
	// sits on the edge in its direction. In a 5v5 the player's radar draws its frame and view (4), site A (1), its four
	// teammates (4) and itself (1), and a frame with nothing new allocates nothing.
	const FVector Origin = FVector::ZeroVector;
	constexpr float Range = 2000.0f;
	constexpr float Half = 40.0f;
	auto Check = [this, &Origin](const TCHAR* What, float Yaw, const FVector& Location, const FVector2D& Expected)
	{
		const FVector2D Offset = AShooterHUD::ProjectToRadar(Origin, Yaw, Location, Range, Half);
		TestTrue(What, Offset.Equals(Expected, 0.01f));
	};
	Check(TEXT("Ahead is up"), 0.0f, FVector(1000.0f, 0.0f, 0.0f), FVector2D(0.0f, -20.0f));
	Check(TEXT("+Y is to the right at yaw 0"), 0.0f, FVector(0.0f, 1000.0f, 0.0f), FVector2D(20.0f, 0.0f));
	Check(TEXT("Behind is down"), 0.0f, FVector(-500.0f, 0.0f, 300.0f), FVector2D(0.0f, 10.0f));
	Check(TEXT("Facing +Y, +Y is up"), 90.0f, FVector(0.0f, 1000.0f, 0.0f), FVector2D(0.0f, -20.0f));
	Check(TEXT("Facing +Y, +X is to the left"), 90.0f, FVector(1000.0f, 0.0f, 0.0f), FVector2D(-20.0f, 0.0f));
	Check(TEXT("Facing -X, +X is down"), 180.0f, FVector(1000.0f, 0.0f, 0.0f), FVector2D(0.0f, 20.0f));
	Check(TEXT("Beyond the range: on the edge"), 0.0f, FVector(10000.0f, 0.0f, 0.0f), FVector2D(0.0f, -40.0f));
	Check(TEXT("In its direction"), 0.0f, FVector(10000.0f, 5000.0f, 0.0f), FVector2D(20.0f, -40.0f));
	Check(TEXT("A corner"), 0.0f, FVector(-9000.0f, -9000.0f, 0.0f), FVector2D(-40.0f, 40.0f));

	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 4, 5);
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::CT);
	TickUntilLive(World, *GameMode);
	TickFrames(World, 2);
	const AShooterHUD* HUD = PaintHUD(*Player);
	if (!TestNotNull("The player's HUD", HUD))
	{
		return false;
	}
	TestEqual("Frame, view, site A, four teammates, the player", HUD->GetNumRadarPrimitives(), 10);
	TestTrue("Cheap", HUD->GetNumRadarPrimitives() <= 20);
	(void)PaintHUD(*Player);
	const uint64 AllocationsBefore = FMemory::GetUsage().TotalAllocations;
	(void)PaintHUD(*Player);
	TestEqual("A frame with nothing new allocates nothing",
		static_cast<int64>(FMemory::GetUsage().TotalAllocations - AllocationsBefore), static_cast<int64>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameHUDDamageIndicatorTest, "ShooterGame.HUD.DamageIndicator",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameHUDDamageIndicatorTest::RunTest(const FString& Parameters)
{
	// The indicator's angle: 0 ahead, 90 to the right, -90 to the left, 180 behind, turning with the view. Shot by an
	// enemy standing to its right, the player's HUD draws the arc toward 90 degrees, and nothing a second later.
	const FVector View(100.0f, 100.0f, 50.0f);
	TestEqual(
		"Ahead", AShooterHUD::GetDamageIndicatorAngle(View, 0.0f, View + FVector(500.0f, 0.0f, 0.0f)), 0.0f, 0.01f);
	TestEqual(
		"Right", AShooterHUD::GetDamageIndicatorAngle(View, 0.0f, View + FVector(0.0f, 500.0f, 80.0f)), 90.0f, 0.01f);
	TestEqual(
		"Left", AShooterHUD::GetDamageIndicatorAngle(View, 0.0f, View + FVector(0.0f, -500.0f, 0.0f)), -90.0f, 0.01f);
	TestEqual("Behind",
		FMath::Abs(AShooterHUD::GetDamageIndicatorAngle(View, 0.0f, View - FVector(500.0f, 0.0f, 0.0f))), 180.0f,
		0.01f);
	TestEqual("Facing +Y, +X is to the left",
		AShooterHUD::GetDamageIndicatorAngle(View, 90.0f, View + FVector(500.0f, 0.0f, 0.0f)), -90.0f, 0.01f);
	TestEqual("Front right", AShooterHUD::GetDamageIndicatorAngle(View, 30.0f, View + FVector(0.0f, 500.0f, 0.0f)),
		60.0f, 0.01f);

	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::CT);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* Pawn = Cast<AShooterCharacter>(Player->GetPawn());
	AShooterCharacter* Enemy = GetAlive(World, EShooterTeam::T)[0];
	if (!TestNotNull("The player plays", Pawn))
	{
		return false;
	}
	// The player faces +X (the CT starts' yaw); the enemy 8 m to its right.
	Enemy->Reset(Pawn->GetActorLocation() + FVector(0.0f, 800.0f, 0.0f), FRotator::ZeroRotator);
	TickFrames(World, 1);
	(void)UGameplayStatics::ApplyDamage(
		Pawn, 10.0f, Enemy->GetController(), Enemy->GetWeapon(), UDamageType::StaticClass());
	TestTrue("Hurt, alive", Pawn->IsAlive() && Pawn->GetHealth() < Pawn->GetMaxHealth());
	TestTrue(
		"The source is the shooter", Player->GetLastDamageSourceLocation().Equals(Enemy->GetActorLocation(), 0.01f));
	TickFrames(World, 1);
	const APlayerCameraManager* Camera = Player->PlayerCameraManager;
	TestEqual("To the right",
		AShooterHUD::GetDamageIndicatorAngle(
			Camera->GetCameraLocation(), Camera->GetCameraRotation().Yaw, Player->GetLastDamageSourceLocation()),
		90.0f, 1.0f);
	const AShooterHUD* HUD = PaintHUD(*Player);
	if (!TestNotNull("The player's HUD", HUD))
	{
		return false;
	}
	TestTrue("The arc shows", HUD->GetNumDamageIndicatorLines() > 0);
	TickSeconds(World, HUD->DamageIndicatorDuration);
	(void)PaintHUD(*Player);
	TestEqual("Gone within its duration", HUD->GetNumDamageIndicatorLines(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBuyAmmoAndPricesTest, "ShooterGame.Buy.AmmoAndPrices",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBuyAmmoAndPricesTest::RunTest(const FString& Parameters)
{
	// CS 1.6's ammunition: a bought weapon comes with its clip only; a box of its calibre costs the calibre's price
	// (9 mm $20 for 30, .45 ACP $25 for 12, .50 AE $40 for 7, 7.62 mm $80 for 30) until the reserve is full; the
	// buy menu's ammo lines and the `,` and `.` keys buy them.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::T);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* Pawn = Cast<AShooterCharacter>(Player->GetPawn());
	AShooterPlayerState* State = Player->GetPlayerState<AShooterPlayerState>();
	if (!TestNotNull("The player plays", Pawn) || !TestNotNull("Its state", State))
	{
		return false;
	}
	State->SetMoney(16000, GameMode->MaxMoney);
	AShooterWeapon* Glock = Pawn->GetWeaponInSlot(EShooterWeaponSlot::Secondary);
	TestTrue("The Glock, 20/40", Glock != nullptr && Glock->GetCurrentAmmo() == 40);
	TestEqual("A box of 9 mm", GameMode->GetPrice(*Pawn, TEXT("secammo")), 20);
	TestEqual("No primary: no primary ammo", GameMode->GetPrice(*Pawn, TEXT("primammo")), -1);
	for (int32 Box = 0; Box < 3; ++Box)
	{
		TestTrue(*FString::Printf(TEXT("Box %d"), Box + 1), GameMode->Buy(Pawn, TEXT("secammo")));
	}
	TestEqual("120: full", Glock->GetCurrentAmmo(), 120);
	FString Reason;
	TestFalse("No fourth box", GameMode->Buy(Pawn, TEXT("secammo"), &Reason));
	TestTrue("The reason", Reason.Contains(TEXT("full")));
	TestEqual("3 x $20", State->GetMoney(), 16000 - 60);

	TestTrue("An AK-47", GameMode->Buy(Pawn, TEXT("ak47")));
	AShooterWeapon* Rifle = Pawn->GetWeaponInSlot(EShooterWeaponSlot::Primary);
	TestTrue("30/0", Rifle != nullptr && Rifle->GetCurrentAmmoInClip() == 30 && Rifle->GetCurrentAmmo() == 0);
	TestEqual("7.62 mm: $80", GameMode->GetPrice(*Pawn, TEXT("primammo")), 80);
	TestTrue("A box from the menu's line", GameMode->Buy(Pawn, TEXT("primammo")));
	TapKey(World, *Player, EKeys::Comma);
	TestEqual("And one with the , key", Rifle->GetCurrentAmmo(), 60);
	TestTrue("The third", GameMode->Buy(Pawn, TEXT("primammo")));
	TestFalse("Full at 90", GameMode->Buy(Pawn, TEXT("primammo")));
	TestEqual("2500 + 3 x 80", State->GetMoney(), 16000 - 60 - 2500 - 240);

	// The other calibres.
	TestTrue("A Desert Eagle", GameMode->Buy(Pawn, TEXT("deagle")));
	TestTrue("The Glock was dropped", Glock->IsDropped());
	const AShooterWeapon* Deagle = Pawn->GetWeaponInSlot(EShooterWeaponSlot::Secondary);
	TestEqual(".50 AE: $40", GameMode->GetPrice(*Pawn, TEXT("secammo")), 40);
	TapKey(World, *Player, EKeys::Period);
	TestTrue("The . key: 7 rounds", Deagle != nullptr && Deagle->GetCurrentAmmo() == 7);
	TestTrue("A USP", GameMode->Buy(Pawn, TEXT("usp")));
	TestEqual(".45 ACP: $25", GameMode->GetPrice(*Pawn, TEXT("secammo")), 25);
	TestTrue("An MP5", GameMode->Buy(Pawn, TEXT("mp5")));
	TestEqual("The MP5's 9 mm: $20", GameMode->GetPrice(*Pawn, TEXT("primammo")), 20);
	TestEqual("The knife is not for sale", GameMode->GetPrice(*Pawn, TEXT("knife")), -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBuyTeamRestrictionsTest, "ShooterGame.Buy.TeamRestrictions",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBuyTeamRestrictionsTest::RunTest(const FString& Parameters)
{
	// CS's team weapons: the AK-47 only for the terrorists, the M4A1 only for the counter-terrorists; the pistols, the
	// MP5 and the AWP for both. The buy menu's pages: 1 Pistols, 2 SMGs, 3 Rifles (the team's), ...; 3 then 1 buys the
	// AK-47 for a terrorist and goes back to the first page.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::T);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* Terrorist = Cast<AShooterCharacter>(Player->GetPawn());
	AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	AShooterPlayerState* State = Player->GetPlayerState<AShooterPlayerState>();
	if (!TestNotNull("The player plays", Terrorist) || !TestNotNull("Its state", State))
	{
		return false;
	}
	State->SetMoney(16000, GameMode->MaxMoney);
	CT->GetController()->GetPlayerState<AShooterPlayerState>()->SetMoney(16000, GameMode->MaxMoney);
	TestEqual("No M4A1 for a T", GameMode->GetPrice(*Terrorist, TEXT("m4a1")), -1);
	TestEqual("No AK-47 for a CT", GameMode->GetPrice(*CT, TEXT("ak47")), -1);
	FString Reason;
	TestFalse("The T cannot buy it", GameMode->Buy(Terrorist, TEXT("m4a1"), &Reason));
	TestTrue("The reason names the team", Reason.Contains(TEXT("only the CT")));
	TestTrue("The CT's rifle", GameMode->Buy(CT, TEXT("m4a1")));
	// For both teams (a pistol already owned is not bought again).
	auto CanBuyOrHas = [GameMode](const AShooterCharacter& Pawn, const TCHAR* Item)
	{
		const AShooterWeapon* Pistol = Pawn.GetWeaponInSlot(EShooterWeaponSlot::Secondary);
		return GameMode->GetPrice(Pawn, Item) > 0 || (Pistol != nullptr && Pistol->WeaponName == Item);
	};
	for (const TCHAR* Both : {TEXT("glock"), TEXT("usp"), TEXT("deagle"), TEXT("mp5"), TEXT("awp")})
	{
		TestTrue(*FString::Printf(TEXT("%s for both"), Both), CanBuyOrHas(*Terrorist, Both) && CanBuyOrHas(*CT, Both));
	}

	// The menu: the categories, then the terrorists' rifles.
	TapKey(World, *Player, EKeys::B);
	TArray<FShooterBuyMenuEntry, TInlineAllocator<AShooterPlayerController::MaxBuyMenuEntries>> Entries;
	Player->GetBuyMenuEntries(Entries);
	TestTrue("Open: the six categories",
		Player->IsBuyMenuOpen() && Entries.Num() == 6 && Entries[2].Category == 2 && Entries[3].Item != nullptr);
	TapKey(World, *Player, EKeys::Three);
	Player->GetBuyMenuEntries(Entries);
	TestTrue("3: the rifles a T may buy",
		Entries.Num() == 2 && FCString::Strcmp(Entries[0].Item, TEXT("ak47")) == 0 &&
			FCString::Strcmp(Entries[1].Item, TEXT("awp")) == 0);
	State->SetTeam(EShooterTeam::CT);
	Player->GetBuyMenuEntries(Entries);
	TestTrue("A CT's", Entries.Num() == 2 && FCString::Strcmp(Entries[0].Item, TEXT("m4a1")) == 0);
	State->SetTeam(EShooterTeam::T);
	TapKey(World, *Player, EKeys::One);
	const AShooterWeapon* Bought = Terrorist->GetWeaponInSlot(EShooterWeaponSlot::Primary);
	TestTrue("1 bought the AK-47", Bought != nullptr && Bought->WeaponName == TEXT("ak47"));
	TestEqual("Back on the first page", Player->GetBuyMenuCategory(), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameGrenadesCarryLimitsTest, "ShooterGame.Grenades.CarryLimits",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameGrenadesCarryLimitsTest::RunTest(const FString& Parameters)
{
	// CS 1.6: two flashbangs ($200 each), one HE and one smoke grenade ($300 each); a grenade bought stays in its slot
	// (the gun stays drawn); the grenade key (4) cycles the HE, the flashbang and the smoke grenade; a flashbang thrown
	// leaves the other. A flashed player's HUD draws the white.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::CT);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* Pawn = Cast<AShooterCharacter>(Player->GetPawn());
	AShooterPlayerState* State = Player->GetPlayerState<AShooterPlayerState>();
	if (!TestNotNull("The player plays", Pawn) || !TestNotNull("Its state", State))
	{
		return false;
	}
	State->SetMoney(16000, GameMode->MaxMoney);
	const AShooterWeapon* Pistol = Pawn->GetWeapon();
	TestEqual("A flashbang: $200", GameMode->GetPrice(*Pawn, TEXT("flashbang")), 200);
	TestTrue("One", GameMode->Buy(Pawn, TEXT("flashbang")));
	const AShooterWeapon* Flashbang = Pawn->FindWeaponOfClass(AShooterWeapon_Flashbang::StaticClass());
	TestTrue("It holds one", Flashbang != nullptr && Flashbang->GetCurrentAmmoInClip() == 1);
	TestTrue("Two", GameMode->Buy(Pawn, TEXT("flashbang")));
	TestTrue("It holds two, the same weapon",
		Pawn->FindWeaponOfClass(AShooterWeapon_Flashbang::StaticClass()) == Flashbang &&
			Flashbang->GetCurrentAmmoInClip() == 2);
	FString Reason;
	TestFalse("Not three", GameMode->Buy(Pawn, TEXT("flashbang"), &Reason));
	TestTrue("The reason", Reason.Contains(TEXT("cannot carry more")));
	TestTrue("An HE", GameMode->Buy(Pawn, TEXT("hegrenade")));
	TestFalse("Not two", GameMode->Buy(Pawn, TEXT("hegrenade")));
	TestTrue("A smoke grenade", GameMode->Buy(Pawn, TEXT("smokegrenade")));
	TestFalse("Not two smokes", GameMode->Buy(Pawn, TEXT("smokegrenade")));
	TestEqual("16000 - 2 x 200 - 300 - 300", State->GetMoney(), 15000);
	TestTrue("The pistol still drawn", Pawn->GetWeapon() == Pistol);

	// The HUD's white: one full-screen tile, as white as the flash.
	Pawn->Flash(1.0f, 1.0f, 0.8f, 0.0f);
	const AShooterHUD* HUD = PaintHUD(*Player);
	TestTrue("The HUD whitens", HUD != nullptr && FMath::IsNearlyEqual(HUD->GetFlashOverlayAlpha(), 0.8f));
	TickSeconds(World, 2.1f);
	HUD = PaintHUD(*Player);
	TestTrue("And clears", HUD != nullptr && HUD->GetFlashOverlayAlpha() == 0.0f);

	auto Drawn = [Pawn]() { return Pawn->GetWeapon() != nullptr ? Pawn->GetWeapon()->WeaponName : FString(); };
	TapKey(World, *Player, EKeys::Four);
	TestEqual("4: the HE", Drawn(), FString(TEXT("hegrenade")));
	TapKey(World, *Player, EKeys::Four);
	TestEqual("4: the flashbang", Drawn(), FString(TEXT("flashbang")));
	TapKey(World, *Player, EKeys::Four);
	TestEqual("4: the smoke grenade", Drawn(), FString(TEXT("smokegrenade")));
	TapKey(World, *Player, EKeys::Four);
	TestEqual("4: the HE again", Drawn(), FString(TEXT("hegrenade")));
	TapKey(World, *Player, EKeys::Four);
	TickSeconds(World, 1.0f);
	Pawn->StartWeaponFire();
	Pawn->StopWeaponFire();
	TestTrue("A flashbang thrown leaves one",
		Pawn->FindWeaponOfClass(AShooterWeapon_Flashbang::StaticClass()) == Flashbang &&
			Flashbang->GetCurrentAmmoInClip() == 1 && Pawn->GetWeapon() == Flashbang);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
