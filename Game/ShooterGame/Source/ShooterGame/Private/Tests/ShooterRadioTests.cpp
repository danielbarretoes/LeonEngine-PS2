#include "CanvasTypes.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/Level.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerStart.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "ShooterAIController.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterHUD.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"
#include "Sound/SoundWave.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// ps2-shipping N30e's radio (AShooterGameMode::SendRadioMessage): what the bots say on the game's events and how their
// teammates act on it, what the HUD shows of it (the viewer's team's, in its colour), and the player's radio menus (CS
// 1.6's Z, X and C) with the radio's limits. The map is the round tests' small open one.

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

	/**
	 * A floor, CT starts at X = -1500 and T starts at X = 1500 in their buy zones, bomb site A at the centre, short
	 * phases; NumCT and NumT bots (the CT first), their brains on.
	 */
	AShooterGameMode* SetUpMatch(UWorld& World, int32 NumCT, int32 NumT)
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
			if (Pawn != nullptr && !Pawn->IsPendingKillPending() && Pawn->IsAlive() && Pawn->GetTeam() == Team &&
				Cast<AShooterAIController>(Pawn->GetController()) != nullptr)
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

	FString GetName(const AShooterCharacter& Pawn)
	{
		return Pawn.GetController()->GetPlayerState<AShooterPlayerState>()->GetPlayerName();
	}

	/** The last radio message of the log that Message is, from Team (any team with None), or null. */
	const FShooterRadioEntry* FindRadio(
		const AShooterGameMode& GameMode, EShooterRadioMessage Message, EShooterTeam Team = EShooterTeam::None)
	{
		const TArray<FShooterRadioEntry>& Log = GameMode.GetShooterGameState()->GetRadioLog();
		for (int32 Index = Log.Num() - 1; Index >= 0; --Index)
		{
			if (Log[Index].Message == Message && (Team == EShooterTeam::None || Log[Index].Team == Team))
			{
				return &Log[Index];
			}
		}
		return nullptr;
	}

	/** Where the bot is going to look (the blackboard's noise), or a far point without one. */
	FVector GetLookAt(const AShooterAIController& Bot)
	{
		return Bot.GetBlackboard().IsValueSet(AShooterAIController::NoiseLocationKey)
			? Bot.GetBlackboard().GetValueAsVector(AShooterAIController::NoiseLocationKey)
			: FVector(1.0e9f, 1.0e9f, 1.0e9f);
	}

	/** A local player's controller on Team (it joins through the game mode). */
	AShooterPlayerController* AddLocalPlayer(UWorld& World, AShooterGameMode& GameMode, EShooterTeam Team)
	{
		AShooterPlayerController* Player = World.SpawnActor<AShooterPlayerController>();
		Player->SetPlayer(NewObject<ULocalPlayer>(Player));
		if (AShooterPlayerState* PlayerState = Player->GetPlayerState<AShooterPlayerState>())
		{
			PlayerState->SetTeam(Team);
			PlayerState->SetPlayerName(TEXT("Player"));
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

	/** A key pressed for a frame, released for a frame. */
	void TapKey(UWorld& World, AShooterPlayerController& Player, const FKey& Key)
	{
		(void)Player.InputKey(Key, IE_Pressed, 1.0f, false);
		TickFrames(World, 1);
		(void)Player.InputKey(Key, IE_Released, 0.0f, false);
		TickFrames(World, 1);
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRadioBotsReportEventsTest, "ShooterGame.Radio.BotsReportEvents",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRadioBotsReportEventsTest::RunTest(const FString& Parameters)
{
	// A CT bot sees a terrorist: "Enemy spotted." with the terrorist's place, and a teammate who cannot see it goes to
	// look there. Hurt below NeedBackupHealth: "Need backup.", which the nearest teammate answers ("Affirmative.")
	// and goes to. The terrorist bot plants: "Bomb has been planted." to its team. A CT player's HUD shows the CT's
	// messages in the CT's colour, not the terrorists'.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 2, 1);
	// Nobody sees anybody until the terrorist is moved (the warmup's sightings would take the radio's first turn).
	SpawnWall(World, 1000.0f, 0.0f, FVector(100.0f, 3000.0f, 400.0f));
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::CT);
	TickUntilLive(World, *GameMode);
	const TArray<AShooterCharacter*> CTs = GetAlive(World, EShooterTeam::CT);
	const TArray<AShooterCharacter*> Ts = GetAlive(World, EShooterTeam::T);
	if (!TestEqual("Two CT bots", CTs.Num(), 2) || !TestEqual("A terrorist bot", Ts.Num(), 1))
	{
		return false;
	}
	AShooterCharacter* Spotter = CTs[0];
	AShooterCharacter* Teammate = CTs[1];
	AShooterCharacter* T = Ts[0];
	Freeze(*T);
	T->SetGodMode(true);
	// The terrorist ahead of the spotter; the teammate 18 m to its side, behind a wall (the bots look where they go).
	SpawnWall(World, -1300.0f, 900.0f, FVector(2600.0f, 100.0f, 400.0f));
	T->Reset(FVector(-700.0f, Spotter->GetActorLocation().Y, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	Teammate->Reset(FVector(-1500.0f, 1500.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f));
	GetBot(*Teammate)->SetControlRotation(FRotator(0.0f, 90.0f, 0.0f));
	TickSeconds(World, 0.5f);
	const FShooterRadioEntry* Spotted = FindRadio(*GameMode, EShooterRadioMessage::EnemySpotted, EShooterTeam::CT);
	TestTrue("Enemy spotted", Spotted != nullptr && Spotted->Team == EShooterTeam::CT);
	if (Spotted == nullptr)
	{
		return false;
	}
	TestEqual("By the spotter", Spotted->SenderName, GetName(*Spotter));
	TestTrue("At the terrorist", Spotted->Location.Equals(T->GetActorLocation(), 1.0f));
	TestNull("The teammate sees nobody", GetBot(*Teammate)->GetEnemy());
	TestTrue("It goes to look there", GetLookAt(*GetBot(*Teammate)).Equals(Spotted->Location, 1.0f));

	// Hurt: the radio lets it speak again 1.5 s after the last message.
	(void)UGameplayStatics::ApplyDamage(Spotter, 70.0f, nullptr, nullptr, UDamageType::StaticClass());
	TickSeconds(World, GameMode->RadioCooldown);
	const FShooterRadioEntry* Backup = FindRadio(*GameMode, EShooterRadioMessage::NeedBackup, EShooterTeam::CT);
	TestTrue("Need backup", Backup != nullptr && Backup->SenderName == GetName(*Spotter));
	const FShooterRadioEntry* Answer = FindRadio(*GameMode, EShooterRadioMessage::Affirmative, EShooterTeam::CT);
	TestTrue("Affirmative, from the teammate", Answer != nullptr && Answer->SenderName == GetName(*Teammate));
	TestTrue(
		"It goes to the spotter", Backup != nullptr && GetLookAt(*GetBot(*Teammate)).Equals(Backup->Location, 1.0f));

	// The CT player's HUD: the CT's lines, "<sender> (RADIO): <message>" in the CT's colour.
	const AShooterHUD* HUD = PaintHUD(*Player);
	if (!TestNotNull("The player's HUD", HUD))
	{
		return false;
	}
	TestTrue("The CT's lines", HUD->GetNumRadioLines() >= 3);
	TestEqual(
		"The first", HUD->GetRadioLineText(0), FString::Printf(TEXT("%s (RADIO): Enemy spotted."), *GetName(*Spotter)));
	const FLinearColor CTColor = HUD->GetRadioLineColor(0);
	TestTrue("In the CT's colour (blue)", CTColor.B > CTColor.R);

	// The terrorist plants: its team hears it, the CT player's HUD does not show it.
	AShooterBomb* Bomb = GameMode->GetBomb();
	T->Reset(FVector(100.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	TickFrames(World, 2);
	TestTrue("Planting", T->StartUse());
	TickSeconds(World, Bomb->PlantDuration);
	const FShooterRadioEntry* Planted = FindRadio(*GameMode, EShooterRadioMessage::BombPlanted);
	TestTrue("Bomb has been planted",
		Planted != nullptr && Planted->Team == EShooterTeam::T && Planted->SenderName == GetName(*T) &&
			Bomb->GetBombState() == EShooterBombState::Planted);
	(void)PaintHUD(*Player);
	bool bShowsTheirs = false;
	for (int32 Index = 0; Index < HUD->GetNumRadioLines(); ++Index)
	{
		bShowsTheirs |= HUD->GetRadioLineText(Index).Contains(TEXT("Bomb has been planted"));
	}
	TestFalse("Not on the CT's HUD", bShowsTheirs);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRadioSectorClearTest, "ShooterGame.Radio.SectorClear",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRadioSectorClearTest::RunTest(const FString& Parameters)
{
	// A teammate's report of an enemy 5 m away: the bot goes there, finds nobody and says "Sector clear."; a bot does
	// not say what a teammate said in the last RadioRepeatTime.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 2, 1);
	SpawnWall(World, 1000.0f, 0.0f, FVector(100.0f, 3000.0f, 400.0f));
	TickUntilLive(World, *GameMode);
	const TArray<AShooterCharacter*> CTs = GetAlive(World, EShooterTeam::CT);
	AShooterCharacter* T = GetAlive(World, EShooterTeam::T)[0];
	Freeze(*T);
	Freeze(*CTs[0]);
	AShooterAIController* Bot = GetBot(*CTs[1]);
	const FVector Spot = CTs[1]->GetActorLocation() + FVector(0.0f, 500.0f, 0.0f);
	TestTrue(
		"The report", GameMode->SendRadioMessage(CTs[0]->GetController(), EShooterRadioMessage::EnemySpotted, &Spot));
	const float ReportTime = World.GetTimeSeconds();
	const AShooterGameState* State = GameMode->GetShooterGameState();
	TestTrue("Heard by the team",
		State->WasRadioSentSince(EShooterTeam::CT, EShooterRadioMessage::EnemySpotted, ReportTime));
	TestFalse(
		"Not by the other", State->WasRadioSentSince(EShooterTeam::T, EShooterRadioMessage::EnemySpotted, ReportTime));
	TestTrue("It goes to look", GetLookAt(*Bot).Equals(Spot, 1.0f));
	const FShooterRadioEntry* Clear = nullptr;
	for (int32 Frame = 0; Frame < 60 * 6 && Clear == nullptr; ++Frame)
	{
		World.Tick(FrameTime);
		Clear = FindRadio(*GameMode, EShooterRadioMessage::SectorClear);
	}
	TestTrue("Sector clear", Clear != nullptr && Clear->SenderName == GetName(*CTs[1]));
	TestTrue("Where it looked", FVector::Dist2D(CTs[1]->GetActorLocation(), Spot) <= Bot->GoalReachedDistance + 10.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRadioPlayerMenuTest, "ShooterGame.Radio.PlayerMenu",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRadioPlayerMenuTest::RunTest(const FString& Parameters)
{
	// CS 1.6's radio keys: C opens radio3 (the HUD lists its nine messages) and 2 sends "Enemy spotted." instead of
	// drawing the pistol; within the 1.5 s cooldown Z and 1 ("Cover me!") are refused; after it they go, and the team's
	// bot answers "Affirmative."; X then Esc closes radio2; the buy menu and a radio menu close each other.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	GameMode->bBotStop = true;
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::CT);
	TickUntilLive(World, *GameMode);
	AShooterCharacter* Pawn = Cast<AShooterCharacter>(Player->GetPawn());
	if (!TestNotNull("The player plays", Pawn))
	{
		return false;
	}
	AShooterWeapon* Knife = Pawn->GetWeaponInSlot(EShooterWeaponSlot::Knife);
	Pawn->EquipWeapon(Knife);
	TickFrames(World, 2);

	TapKey(World, *Player, EKeys::C);
	TestEqual("C: radio3", Player->GetRadioMenu(), 3);
	const AShooterHUD* HUD = PaintHUD(*Player);
	TestTrue("Its title and nine messages", HUD != nullptr && HUD->GetNumRadioMenuLines() == 10);
	TapKey(World, *Player, EKeys::Two);
	const FShooterRadioEntry* Spotted = FindRadio(*GameMode, EShooterRadioMessage::EnemySpotted, EShooterTeam::CT);
	TestTrue("2: Enemy spotted",
		Spotted != nullptr && Spotted->Team == EShooterTeam::CT &&
			Spotted->SenderName == Player->GetPlayerState<AShooterPlayerState>()->GetPlayerName());
	TestEqual("The menu closed", Player->GetRadioMenu(), 0);
	TestTrue("The pistol not drawn", Pawn->GetWeapon() == Knife);

	TapKey(World, *Player, EKeys::Z);
	TestEqual("Z: radio1", Player->GetRadioMenu(), 1);
	TapKey(World, *Player, EKeys::One);
	TestNull("Too soon: refused", FindRadio(*GameMode, EShooterRadioMessage::CoverMe));
	TickSeconds(World, GameMode->RadioCooldown);
	TapKey(World, *Player, EKeys::Z);
	TapKey(World, *Player, EKeys::One);
	const FShooterRadioEntry* Cover = FindRadio(*GameMode, EShooterRadioMessage::CoverMe);
	TestTrue("Then: Cover me!", Cover != nullptr);
	const FShooterRadioEntry* Answer = FindRadio(*GameMode, EShooterRadioMessage::Affirmative, EShooterTeam::CT);
	TestTrue("The bot answers", Answer != nullptr && Answer->Team == EShooterTeam::CT);

	TapKey(World, *Player, EKeys::X);
	TestEqual("X: radio2", Player->GetRadioMenu(), 2);
	TapKey(World, *Player, EKeys::Escape);
	TestEqual("Esc closes it", Player->GetRadioMenu(), 0);

	TapKey(World, *Player, EKeys::B);
	TestTrue("B: the buy menu", Player->IsBuyMenuOpen());
	TapKey(World, *Player, EKeys::C);
	TestTrue("C closes it for radio3", !Player->IsBuyMenuOpen() && Player->GetRadioMenu() == 3);
	TapKey(World, *Player, EKeys::C);
	TestEqual("C again closes radio3", Player->GetRadioMenu(), 0);
	TestTrue("The weapons' keys are the pawn's again", Pawn->GetWeapon() == Knife);
	TapKey(World, *Player, EKeys::Two);
	TickFrames(World, 2);
	TestTrue("2 draws the pistol", Pawn->GetWeapon() == Pawn->GetWeaponInSlot(EShooterWeaponSlot::Secondary));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameRadioSoundsTest, "ShooterGame.Radio.Sounds",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameRadioSoundsTest::RunTest(const FString& Parameters)
{
	// ps2-shipping N30f: a radio message plays its sound to the local players of the sender's team (DefaultGame.ini's
	// Radio*SoundName): a menu's sound for its messages, "Fire in the hole!" and "Bomb has been planted." their own;
	// the other team hears nothing.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	GameMode->bBotStop = true;
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::CT);
	TickUntilLive(World, *GameMode);
	auto SoundName = [](const USoundWave* Sound) { return Sound != nullptr ? Sound->GetName() : FString(); };
	TestEqual("radio1: the commands", SoundName(Player->GetRadioSound(EShooterRadioMessage::CoverMe)),
		FString(TEXT("S_Radio_Command")));
	TestEqual("radio2: the group's", SoundName(Player->GetRadioSound(EShooterRadioMessage::GoGoGo)),
		FString(TEXT("S_Radio_Group")));
	TestEqual("radio3: the reports", SoundName(Player->GetRadioSound(EShooterRadioMessage::EnemyDown)),
		FString(TEXT("S_Radio_Report")));
	TestEqual("Fire in the hole!", SoundName(Player->GetRadioSound(EShooterRadioMessage::FireInTheHole)),
		FString(TEXT("S_Radio_FireInTheHole")));
	TestEqual("Bomb has been planted.", SoundName(Player->GetRadioSound(EShooterRadioMessage::BombPlanted)),
		FString(TEXT("S_Radio_BombPlanted")));
	TestNull("None: silent", Player->GetRadioSound(EShooterRadioMessage::None));

	const TArray<AShooterCharacter*> Ts = GetAlive(World, EShooterTeam::T);
	const TArray<AShooterCharacter*> CTs = GetAlive(World, EShooterTeam::CT);
	if (!TestTrue("A bot a team", Ts.Num() == 1 && CTs.Num() == 1))
	{
		return false;
	}
	TestTrue(
		"A teammate's news", GameMode->SendRadioMessage(CTs[0]->GetController(), EShooterRadioMessage::BombPlanted));
	TestEqual("Heard", SoundName(Player->GetLastRadioSound()), FString(TEXT("S_Radio_BombPlanted")));
	TestTrue("The other team's grenade",
		GameMode->SendRadioMessage(Ts[0]->GetController(), EShooterRadioMessage::FireInTheHole));
	TestEqual("Not heard", SoundName(Player->GetLastRadioSound()), FString(TEXT("S_Radio_BombPlanted")));
	TestTrue("A teammate's grenade",
		GameMode->SendRadioMessage(CTs[0]->GetController(), EShooterRadioMessage::FireInTheHole));
	TestEqual("Heard", SoundName(Player->GetLastRadioSound()), FString(TEXT("S_Radio_FireInTheHole")));
	TickSeconds(World, GameMode->RadioCooldown);
	TestTrue("The player's own report", Player->SendRadio(EShooterRadioMessage::SectorClear));
	TestEqual("Heard too", SoundName(Player->GetLastRadioSound()), FString(TEXT("S_Radio_Report")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
