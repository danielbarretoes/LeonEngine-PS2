#include "AI/Navigation/NavigationSystem.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/AutomationTest.h"
#include "ShooterAIController.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"
#include "UObject/StrongObjectPtr.h"
#include "Weapons/ShooterWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS

// P20's tests: the bots' decisions (buying, engaging, planting, defusing) on a small open map, and a headless match on
// de_leon with a fixed seed whose rounds keep the game's invariants.

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
	 * The open test map of the round tests: a floor, CT starts at X = -1500, T starts at X = 1500, their buy zones,
	 * bomb site A at the centre; short phases; NumCT and NumT bots with their brains on.
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
		GameMode->bFillTeamsWithBots = false;
		GameMode->FreezeTime = 0.5f;
		GameMode->RoundTime = 60.0f;
		GameMode->RoundRestartDelay = 0.5f;
		GameMode->BuyTime = 3.0f;
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
			Bot->bCanEverTick = false;
		}
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsBuyTest, "ShooterGame.Bots.Buy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsBuyTest::RunTest(const FString& Parameters)
{
	// A bot buys by its money: $800 a vest; $3500 a rifle and kevlar with a helmet; a CT with $1400 kevlar, a
	// helmet and a kit; once a round.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpBotMatch(World, 1, 1);
	GameMode->bBotStop = true;
	TickFrames(World, 1);
	AShooterCharacter* T = GetAlive(World, EShooterTeam::T)[0];
	AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	AShooterAIController* TBot = Cast<AShooterAIController>(T->GetController());
	AShooterAIController* CTBot = Cast<AShooterAIController>(CT->GetController());
	if (!TestNotNull("T bot", TBot) || !TestNotNull("CT bot", CTBot))
	{
		return false;
	}
	TArray<FString> Bought = TBot->BuyForRound();
	TestTrue("$800: a vest", Bought.Num() == 1 && Bought[0] == TEXT("vest"));
	TestEqual("Once a round", TBot->BuyForRound().Num(), 0);

	GameMode->EndRound(EShooterRoundEndReason::Draw);
	TickFrames(World, 40);
	AShooterPlayerState* TState = TBot->GetPlayerState<AShooterPlayerState>();
	TState->SetMoney(3500, GameMode->MaxMoney);
	CTBot->GetPlayerState<AShooterPlayerState>()->SetMoney(1400, GameMode->MaxMoney);
	Bought = TBot->BuyForRound();
	TestTrue("$3500: a rifle", Bought.Contains(TEXT("ak47")) || Bought.Contains(TEXT("awp")));
	TestTrue("$3500: the helmet too", Bought.Contains(TEXT("vesthelm")) || T->HasHelmet());
	Bought = CTBot->BuyForRound();
	TestTrue(
		"A CT with $1400: kevlar, helmet, kit", Bought.Contains(TEXT("vesthelm")) && Bought.Contains(TEXT("defuser")));
	TestTrue("The kit", CT->HasDefuseKit());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsEngageTest, "ShooterGame.Bots.EngageKillsAnEnemy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsEngageTest::RunTest(const FString& Parameters)
{
	// A CT bot sees a still terrorist ahead: it turns to it, waits its reaction time, then fires until the terrorist
	// is dead; the round ends with the CT's win.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpBotMatch(World, 1, 1);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* T = GetAlive(World, EShooterTeam::T)[0];
	AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	Freeze(*T);
	T->Reset(FVector(-700.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	AShooterAIController* Bot = Cast<AShooterAIController>(CT->GetController());
	const AShooterWeapon* Weapon = CT->GetWeapon();
	const int32 ShotsBefore = Weapon != nullptr ? Weapon->GetShotsFired() : 0;
	float FirstShotTime = -1.0f;
	float SeenTime = -1.0f;
	for (int32 Frame = 0; Frame < 60 * 10 && T->IsAlive(); ++Frame)
	{
		World.Tick(FrameTime);
		if (SeenTime < 0.0f && Bot->GetEnemy() == T)
		{
			SeenTime = World.GetTimeSeconds();
		}
		if (FirstShotTime < 0.0f && CT->GetWeapon() != nullptr && CT->GetWeapon()->GetShotsFired() > ShotsBefore)
		{
			FirstShotTime = World.GetTimeSeconds();
		}
	}
	TestTrue("It saw the terrorist", SeenTime >= 0.0f);
	TestTrue("It engaged", Bot->GetCurrentTask() == FName(TEXT("Engage")) || !T->IsAlive());
	TestTrue(
		"Not before its reaction time", FirstShotTime < 0.0f || FirstShotTime - SeenTime >= Bot->ReactionTime - 0.02f);
	TestFalse("The terrorist is dead", T->IsAlive());
	TickFrames(World, 2);
	TestTrue("CT win",
		GameMode->GetShooterGameState()->GetLastRoundEndReason() == EShooterRoundEndReason::TerroristsEliminated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsPlantTest, "ShooterGame.Bots.CarrierPlants",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsPlantTest::RunTest(const FString& Parameters)
{
	// The terrorist carrying the bomb walks to the round's site and plants it; the CT is kept out of sight.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpBotMatch(World, 1, 1);
	SpawnWall(World, -2600.0f, 0.0f, FVector(100.0f, 1000.0f, 400.0f));
	TickUntilLive(World, *GameMode);
	AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	Freeze(*CT);
	CT->Reset(FVector(-3000.0f, 0.0f, 0.0f));
	TestTrue("The site", GameMode->GetTerroristTargetSite() == FName(TEXT("A")));
	AShooterBomb* Bomb = GameMode->GetBomb();
	for (int32 Frame = 0; Frame < 60 * 20 && Bomb->GetBombState() != EShooterBombState::Planted; ++Frame)
	{
		World.Tick(FrameTime);
	}
	TestTrue("Planted", Bomb->GetBombState() == EShooterBombState::Planted);
	TestTrue("At A", Bomb->GetSite() == FName(TEXT("A")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsDefuseTest, "ShooterGame.Bots.CTDefuses",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsDefuseTest::RunTest(const FString& Parameters)
{
	// A planted bomb and no terrorist left: the CT bot walks to the bomb and defuses it (with a kit, 5 s).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpBotMatch(World, 1, 1);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* T = GetAlive(World, EShooterTeam::T)[0];
	AShooterCharacter* CT = GetAlive(World, EShooterTeam::CT)[0];
	Freeze(*T);
	Freeze(*CT);
	AShooterBomb* Bomb = GameMode->GetBomb();
	T->Reset(FVector(100.0f, 0.0f, 0.0f));
	TickFrames(World, 2);
	TestTrue("Planting", T->StartUse());
	TickFrames(World, 60 * 4);
	TestTrue("Planted", Bomb->GetBombState() == EShooterBombState::Planted);
	T->Suicide();
	CT->SetDefuseKit(true);
	AShooterAIController* Bot = Cast<AShooterAIController>(CT->GetController());
	Bot->bCanEverTick = true;
	bool bDefusing = false;
	for (int32 Frame = 0; Frame < 60 * 30 && Bomb->GetBombState() == EShooterBombState::Planted; ++Frame)
	{
		World.Tick(FrameTime);
		bDefusing |= CT->IsDefusing();
	}
	TestTrue("It defused", bDefusing && Bomb->GetBombState() == EShooterBombState::Defused);
	TestTrue("Its task",
		Bot->GetCurrentTask() == FName(TEXT("Defuse")) || Bomb->GetBombState() == EShooterBombState::Defused);
	TickFrames(World, 2);
	TestTrue("CT win", GameMode->GetShooterGameState()->GetLastRoundEndReason() == EShooterRoundEndReason::BombDefused);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameBotsMatchOnDeLeonTest, "ShooterGame.Bots.MatchOnDeLeon",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameBotsMatchOnDeLeonTest::RunTest(const FString& Parameters)
{
	// Ten bots play three rounds of de_leon, headless, seed 5, at 60 Hz: every round ends with a reason and a winner
	// that matches it, the scores add up, the money stays within [0, 16000], no pawn falls through the floor, the
	// waypoint graph is there, and the bots fight (kills happen).
	TStrongObjectPtr<UGameEngine> Engine(NewObject<UGameEngine>());
	Engine->Init(nullptr);
	FWorldContext& Context = *Engine->GameInstance->GetWorldContext();
	FString Error;
	if (!TestEqual("Browse",
			static_cast<int32>(
				Engine->Browse(Context, FURL(nullptr, TEXT("/Game/Maps/de_leon?seed=5"), TRAVEL_Absolute), Error)),
			static_cast<int32>(EBrowseReturnVal::Success)))
	{
		AddError(Error);
		Engine->PreExit();
		return false;
	}
	UWorld* World = Engine->GetGameWorld();
	AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	if (!TestNotNull("ShooterGameMode", GameMode))
	{
		Engine->PreExit();
		return false;
	}
	TestTrue("The waypoint graph", World->GetNavigationSystem().GetNodes().Num() >= 10);
	TestEqual("The seed", GameMode->RandomSeed, 5);
	// The player joined CT; the bots fill both teams (the player's pawn stands still).
	(void)GameMode->FillTeamsWithBots();
	AShooterGameState* State = GameMode->GetShooterGameState();
	constexpr int32 RoundsToPlay = 3;
	const int32 MaxFrames = RoundsToPlay *
		static_cast<int32>(
			(GameMode->FreezeTime + GameMode->RoundTime + 45.0f + GameMode->RoundRestartDelay + 2.0f) * 60.0f);
	TArray<EShooterRoundEndReason> Reasons;
	int32 LastRound = 0;
	bool bMoneyInRange = true;
	bool bAboveFloor = true;
	for (int32 Frame = 0; Frame < MaxFrames && Reasons.Num() < RoundsToPlay; ++Frame)
	{
		Engine->Tick(FrameTime, false);
		if (State->GetRoundState() == EShooterRoundState::RoundEnd && Reasons.Num() < State->GetRoundNumber() &&
			LastRound != State->GetRoundNumber())
		{
			LastRound = State->GetRoundNumber();
			Reasons.Add(State->GetLastRoundEndReason());
			UE_LOG(LogTemp, Display, TEXT("MatchOnDeLeon: round %d: %s (CT %d - T %d), %d kill(s)"), LastRound,
				GetRoundEndMessage(State->GetLastRoundEndReason()), State->GetTeamScore(EShooterTeam::CT),
				State->GetTeamScore(EShooterTeam::T), GameMode->GetNumKills());
		}
		if (Frame % 30 == 0)
		{
			for (const APlayerState* PlayerState : GameMode->GetGameState().GetPlayerArray())
			{
				const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState);
				bMoneyInRange &= ShooterState == nullptr ||
					(ShooterState->GetMoney() >= 0 && ShooterState->GetMoney() <= GameMode->MaxMoney);
			}
			for (const AShooterCharacter* Pawn : GetAlive(*World, EShooterTeam::CT))
			{
				bAboveFloor &= Pawn->GetActorLocation().Z > -20.0f;
			}
			for (const AShooterCharacter* Pawn : GetAlive(*World, EShooterTeam::T))
			{
				bAboveFloor &= Pawn->GetActorLocation().Z > -20.0f;
			}
		}
	}
	TestEqual("Three rounds played", Reasons.Num(), RoundsToPlay);
	int32 Decided = 0;
	for (const EShooterRoundEndReason Reason : Reasons)
	{
		TestTrue("A reason", Reason != EShooterRoundEndReason::None);
		Decided += GetRoundEndWinner(Reason) != EShooterTeam::None ? 1 : 0;
	}
	TestEqual(
		"The scores add up", State->GetTeamScore(EShooterTeam::CT) + State->GetTeamScore(EShooterTeam::T), Decided);
	TestTrue("The money in range", bMoneyInRange);
	TestTrue("Nobody fell through the floor", bAboveFloor);
	TestTrue("The bots fought", GameMode->GetNumKills() > 0);
	Engine->PreExit();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
