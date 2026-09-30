#include "CanvasTypes.h"
#include "Components/Border.h"
#include "Components/TableView.h"
#include "Components/TextBlock.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/Font.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "InputCoreTypes.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "ShooterBomb.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterHUD.h"
#include "ShooterPersistentUser.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"
#include "ShooterScoreboardWidget.h"
#include "Tests/ScopedTestWorld.h"
#include "Weapons/ShooterWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS

// ps2-polish P6's tests: the HUD's icons and the weapons' display names, the kill feed's icons, the scoreboard's
// tables, the buy menu's table and the Fire key planting the C4.

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

	ATriggerVolume* SpawnZone(
		UWorld& World, const FVector& Center, const FVector& Size, const TCHAR* Kind, const TCHAR* Name)
	{
		ATriggerVolume* Zone = World.SpawnActor<ATriggerVolume>(
			ATriggerVolume::StaticClass(), FTransform(FQuat::Identity, Center, Size / 100.0f));
		Zone->Tags.Add(FName(Kind));
		Zone->Tags.Add(FName(Name));
		return Zone;
	}

	/** The round tests' map: a floor, five starts a team in their buy zones, bomb site A at the centre; short phases.
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
		GameMode->bBotStop = true;
		GameMode->FreezeTime = 0.5f;
		GameMode->RoundTime = 20.0f;
		GameMode->RoundRestartDelay = 0.5f;
		GameMode->BuyTime = 3.0f;
		GameMode->RandomSeed = 7;
		(void)GameMode->AddBots(EShooterTeam::CT, NumCT);
		(void)GameMode->AddBots(EShooterTeam::T, NumT);
		return GameMode;
	}

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

	void TickUntil(UWorld& World, const AShooterGameMode& GameMode, EShooterRoundState RoundState)
	{
		for (int32 Frame = 0; Frame < 600; ++Frame)
		{
			const AShooterGameState* State = GameMode.GetShooterGameState();
			if (State != nullptr && State->GetRoundState() == RoundState)
			{
				return;
			}
			World.Tick(FrameTime);
		}
	}

	/** The HUD's frame as the canvas's primitives. */
	struct FHUDFrame
	{
		TArray<FCanvasVertex> Vertices;
		TArray<FCanvasPrimitiveRun> Runs;

		explicit FHUDFrame(AShooterHUD& HUD)
		{
			FCanvas Canvas(640, 448);
			HUD.Paint(Canvas);
			Canvas.GetPrimitives(Vertices, Runs);
		}

		/** Whether a rectangle of Texture starts at the texel (U, V) of a SizeX x SizeY texture. */
		bool HasRectangleAt(const UTexture2D* Texture, float U, float V) const
		{
			const float SizeX = float(Texture->GetSizeX());
			const float SizeY = float(Texture->GetSizeY());
			for (const FCanvasPrimitiveRun& Run : Runs)
			{
				if (Run.Texture != Texture || Run.Type != ECanvasPrimitive::Rectangle)
				{
					continue;
				}
				for (int32 Index = Run.FirstVertex; Index + 1 < Run.FirstVertex + Run.NumVertices; Index += 2)
				{
					if (FMath::IsNearlyEqual(Vertices[Index].U, U / SizeX, 1.0e-4f) &&
						FMath::IsNearlyEqual(Vertices[Index].V, V / SizeY, 1.0e-4f))
					{
						return true;
					}
				}
			}
			return false;
		}
	};

	/** Whether the icon's rectangle of the loaded atlas is drawn in the frame. */
	bool DrawsIcon(const FHUDFrame& Frame, const AShooterHUD& HUD, EShooterHUDIcon Icon)
	{
		float U = 0.0f;
		float V = 0.0f;
		float UL = 0.0f;
		float VL = 0.0f;
		AShooterHUD::GetIconRect(Icon, U, V, UL, VL);
		return HUD.GetIconsTexture() != nullptr && Frame.HasRectangleAt(HUD.GetIconsTexture(), U, V);
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameHUDIconAtlasTest, "ShooterGame.HUD.IconAtlas",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameHUDIconAtlasTest::RunTest(const FString& Parameters)
{
	// The atlas (/Game/UI/T_HUDIcons, SourceArt/UI/make_hud_icons.py) and the HUD's table agree: every icon's
	// rectangle holds opaque texels, and no texel outside the rectangles is drawn. The atlas is white, its coverage in
	// the alpha (16 levels: the cook makes it PSMT4).
	const UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/T_HUDIcons.T_HUDIcons"));
	if (!TestNotNull("The atlas", Atlas) || !TestEqual("RGBA", int32(Atlas->GetPixelFormat()), int32(PF_R8G8B8A8)))
	{
		return false;
	}
	const int32 SizeX = Atlas->GetSizeX();
	const int32 SizeY = Atlas->GetSizeY();
	TestTrue("256 x 64", SizeX == 256 && SizeY == 64);
	const FByteBulkData& BulkData = Atlas->GetPlatformData().Mips[0].BulkData;
	const uint8* Texels = static_cast<const uint8*>(BulkData.LockReadOnly());
	// Bottom row first: the texel (X, Y from the top).
	auto Alpha = [Texels, SizeX, SizeY](int32 X, int32 Y) { return Texels[(((SizeY - 1 - Y) * SizeX) + X) * 4 + 3]; };
	TArray<bool> Covered;
	Covered.SetNumZeroed(SizeX * SizeY);
	TSet<uint8> Levels;
	bool bWhite = true;
	for (int32 Icon = 0; Icon < static_cast<int32>(EShooterHUDIcon::Num); ++Icon)
	{
		float U = 0.0f;
		float V = 0.0f;
		float UL = 0.0f;
		float VL = 0.0f;
		AShooterHUD::GetIconRect(static_cast<EShooterHUDIcon>(Icon), U, V, UL, VL);
		int32 Opaque = 0;
		for (int32 Y = int32(V); Y < int32(V + VL); ++Y)
		{
			for (int32 X = int32(U); X < int32(U + UL); ++X)
			{
				Covered[(Y * SizeX) + X] = true;
				Opaque += Alpha(X, Y) == 255 ? 1 : 0;
			}
		}
		TestTrue(*FString::Printf(TEXT("Icon %d has its shape"), Icon), Opaque > 8);
	}
	int32 Stray = 0;
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			const uint8 Level = Alpha(X, Y);
			Levels.Add(Level);
			Stray += !Covered[(Y * SizeX) + X] && Level != 0 ? 1 : 0;
			const uint8* Texel = &Texels[(((SizeY - 1 - Y) * SizeX) + X) * 4];
			bWhite &= Texel[0] == 255 && Texel[1] == 255 && Texel[2] == 255;
		}
	}
	BulkData.Unlock();
	TestEqual("Nothing outside the icons", Stray, 0);
	TestTrue("White texels", bWhite);
	TestTrue("At most 16 alphas (PSMT4)", Levels.Num() <= 16);
	TestEqual(
		"A kill icon is 16 texels high",
		[]
		{
			float U = 0.0f;
			float V = 0.0f;
			float UL = 0.0f;
			float VL = 0.0f;
			AShooterHUD::GetIconRect(EShooterHUDIcon::AK47, U, V, UL, VL);
			return VL;
		}(),
		16.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameHUDDisplayNamesTest, "ShooterGame.HUD.DisplayNames",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameHUDDisplayNamesTest::RunTest(const FString& Parameters)
{
	// CS's names for what the player reads: every weapon's DisplayName, the items' (the bomb, the equipment, the
	// ammunition), and a name without one kept as it is.
	const TCHAR* const Expected[][2] = {
		{TEXT("ak47"), TEXT("AK-47")},
		{TEXT("m4a1"), TEXT("M4A1")},
		{TEXT("usp"), TEXT("USP")},
		{TEXT("deagle"), TEXT("Desert Eagle")},
		{TEXT("glock"), TEXT("Glock-18")},
		{TEXT("mp5"), TEXT("MP5")},
		{TEXT("awp"), TEXT("AWP")},
		{TEXT("hegrenade"), TEXT("HE Grenade")},
		{TEXT("flashbang"), TEXT("Flashbang")},
		{TEXT("smokegrenade"), TEXT("Smoke Grenade")},
		{TEXT("knife"), TEXT("Knife")},
		{TEXT("c4"), TEXT("C4")},
		{TEXT("vesthelm"), TEXT("Kevlar + Helmet")},
		{TEXT("defuser"), TEXT("Defuse Kit")},
		{TEXT("primammo"), TEXT("Primary Ammo")},
		{TEXT("AK47"), TEXT("AK-47")},
		{TEXT("nothing"), TEXT("nothing")},
	};
	for (const auto& Pair : Expected)
	{
		TestEqual(Pair[0], AShooterWeapon::GetItemDisplayName(Pair[0]), FString(Pair[1]));
	}
	TArray<UClass*> Classes;
	AShooterWeapon::GetWeaponClasses(Classes);
	TestEqual("Eleven weapons", Classes.Num(), 11);
	for (UClass* Class : Classes)
	{
		const AShooterWeapon* Weapon = Class->GetDefaultObject<AShooterWeapon>();
		TestFalse(*(Weapon->WeaponName + TEXT(": a display name")), Weapon->DisplayName.IsEmpty());
		TestTrue(*(Weapon->WeaponName + TEXT(": a kill feed icon")),
			AShooterHUD::GetKillFeedIcon(Weapon->WeaponName) != EShooterHUDIcon::World);
	}
	TestTrue("The bomb's icon", AShooterHUD::GetKillFeedIcon(TEXT("c4")) == EShooterHUDIcon::C4);
	TestTrue("The world's: the skull", AShooterHUD::GetKillFeedIcon(TEXT("world")) == EShooterHUDIcon::World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameHUDKillFeedIconsTest, "ShooterGame.HUD.KillFeedIcons",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameHUDKillFeedIconsTest::RunTest(const FString& Parameters)
{
	// The kill feed draws each kill's weapon as its icon between the names, the headshot's after it, and the world's
	// skull for a death without a weapon; the HUD's status draws the health's and the armor's icons and the numbers in
	// the bold font, the text with its shadow.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::CT);
	TickUntil(World, *GameMode, EShooterRoundState::Live);
	AShooterHUD* HUD = Cast<AShooterHUD>(Player->MyHUD);
	if (!TestNotNull("The player's HUD", HUD) || !TestNotNull("Its icons", HUD->GetIconsTexture()))
	{
		return false;
	}
	AShooterGameState* State = GameMode->GetShooterGameState();
	auto AddKill = [&World, State](const TCHAR* Killer, const TCHAR* Weapon, bool bHeadshot)
	{
		FShooterKillFeedEntry Entry;
		Entry.KillerName = Killer;
		Entry.VictimName = TEXT("Victim");
		Entry.WeaponName = Weapon;
		Entry.bHeadshot = bHeadshot;
		Entry.KillerTeam = EShooterTeam::CT;
		Entry.VictimTeam = EShooterTeam::T;
		Entry.Time = World.GetTimeSeconds();
		State->AddKillFeedEntry(Entry);
	};
	{
		const FHUDFrame Before(*HUD);
		TestFalse("No kill: no AK-47", DrawsIcon(Before, *HUD, EShooterHUDIcon::AK47));
		TestTrue("The health's cross", DrawsIcon(Before, *HUD, EShooterHUDIcon::Health));
		TestTrue("The armor's vest", DrawsIcon(Before, *HUD, EShooterHUDIcon::Armor));
		TestTrue("The clock's stopwatch", DrawsIcon(Before, *HUD, EShooterHUDIcon::Clock));
	}
	AddKill(TEXT("Killer"), TEXT("ak47"), true);
	AddKill(TEXT("Thrower"), TEXT("hegrenade"), false);
	AddKill(TEXT(""), TEXT("world"), false);
	const FHUDFrame Frame(*HUD);
	TestEqual("Three lines", HUD->GetNumKillFeedLines(), 3);
	TestTrue("The AK-47's icon", DrawsIcon(Frame, *HUD, EShooterHUDIcon::AK47));
	TestTrue("The headshot's", DrawsIcon(Frame, *HUD, EShooterHUDIcon::Headshot));
	TestTrue("The HE grenade's", DrawsIcon(Frame, *HUD, EShooterHUDIcon::HEGrenade));
	TestTrue("The world's skull", DrawsIcon(Frame, *HUD, EShooterHUDIcon::World));
	TestFalse("No other weapon", DrawsIcon(Frame, *HUD, EShooterHUDIcon::AWP));
	// The status's numbers in the bold font's page, a shadow copy under each glyph.
	const UFont* Numbers = HUD->GetNumberFont();
	int32 NumberGlyphs = 0;
	for (const FCanvasPrimitiveRun& Run : Frame.Runs)
	{
		NumberGlyphs += Numbers != nullptr && Numbers->Textures.Contains(Run.Texture) ? Run.NumVertices / 2 : 0;
	}
	TestTrue("Numbers in the bold font, shadowed (an even count)", NumberGlyphs > 8 && NumberGlyphs % 2 == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameHUDScoreboardTest, "ShooterGame.HUD.Scoreboard",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameHUDScoreboardTest::RunTest(const FString& Parameters)
{
	// The scoreboard's tables (UTableView): a team each with its players, most kills first (ties: fewer deaths first),
	// the dead marked DEAD, a bot's latency BOT, the local player's row highlighted, the team's score over its table;
	// the bomb's carrier is marked BOMB for a terrorist, not for a counter-terrorist. The tables are filled again only
	// when what they show changed.
	for (const EShooterTeam ViewerTeam : {EShooterTeam::CT, EShooterTeam::T})
	{
		const FString Side = ViewerTeam == EShooterTeam::CT ? TEXT("CT viewer: ") : TEXT("T viewer: ");
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		AShooterGameMode* GameMode = ViewerTeam == EShooterTeam::CT ? SetUpMatch(World, 4, 5) : SetUpMatch(World, 5, 4);
		AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, ViewerTeam);
		TickUntil(World, *GameMode, EShooterRoundState::Live);
		AShooterHUD* HUD = Cast<AShooterHUD>(Player->MyHUD);
		UShooterScoreboardWidget* Board = HUD != nullptr ? HUD->GetScoreboardWidget() : nullptr;
		if (!TestNotNull(*(Side + TEXT("the scoreboard")), Board))
		{
			return false;
		}
		TickFrames(World, 1);
		TestTrue(
			*(Side + TEXT("hidden without Tab")), Board->GetPanel()->GetVisibility() == ESlateVisibility::Collapsed);

		// Scores: a CT bot with two kills, the player with one; a T dies.
		AShooterPlayerState* Local = Player->GetPlayerState<AShooterPlayerState>();
		AShooterPlayerState* TopCT = nullptr;
		AShooterCharacter* DeadT = nullptr;
		for (APlayerState* State : GameMode->GetGameState().GetPlayerArray())
		{
			AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(State);
			if (ShooterState != nullptr && ShooterState != Local && ShooterState->GetTeam() == EShooterTeam::CT &&
				TopCT == nullptr)
			{
				TopCT = ShooterState;
			}
		}
		for (AShooterCharacter* Pawn : GameMode->GetPawns())
		{
			if (Pawn->GetTeam() == EShooterTeam::T && Pawn->GetController() != Player &&
				Pawn->GetCarriedBomb() == nullptr && DeadT == nullptr)
			{
				DeadT = Pawn;
			}
		}
		if (!TestNotNull(*(Side + TEXT("a CT bot")), TopCT) || !TestNotNull(*(Side + TEXT("a T to die")), DeadT))
		{
			return false;
		}
		TopCT->ScoreKill(1);
		TopCT->ScoreKill(1);
		Local->ScoreKill(1);
		const FString DeadName = DeadT->GetController()->GetPlayerState<APlayerState>()->GetPlayerName();
		DeadT->Suicide();
		Player->ShowScores(1);
		TickFrames(World, 1);
		TestTrue(*(Side + TEXT("shown")), Board->GetPanel()->GetVisibility() == ESlateVisibility::Visible);
		const UTableView* CT = Board->GetTeamTable(EShooterTeam::CT);
		const UTableView* T = Board->GetTeamTable(EShooterTeam::T);
		TestTrue(*(Side + TEXT("five a team")), CT->GetNumRows() == 5 && T->GetNumRows() == 5);
		TestTrue(*(Side + TEXT("the columns")),
			CT->GetNumColumns() == 5 && CT->GetColumn(2).ColumnId == UShooterScoreboardWidget::ScoreColumn);
		// Sorted by score: the numbers never go up down the table.
		for (const UTableView* Table : {CT, T})
		{
			for (int32 Row = 1; Row < Table->GetNumRows(); ++Row)
			{
				TestTrue(*(Side + TEXT("sorted by score")),
					FCString::Atoi(*Table->GetCellText(Row - 1, 2)) >= FCString::Atoi(*Table->GetCellText(Row, 2)));
			}
		}
		if (ViewerTeam == EShooterTeam::CT)
		{
			TestEqual(*(Side + TEXT("the top scorer first")), CT->GetCellText(0, 0), TopCT->GetPlayerName());
			TestEqual(*(Side + TEXT("then the player")), CT->GetCellText(1, 0), Local->GetPlayerName());
		}
		const UTableView* Own = ViewerTeam == EShooterTeam::CT ? CT : T;
		const int32 Highlighted = Own->GetHighlightedRow();
		TestTrue(*(Side + TEXT("the player's row highlighted")),
			Highlighted != INDEX_NONE && Own->GetCellText(Highlighted, 0) == Local->GetPlayerName());
		TestEqual(*(Side + TEXT("the player's latency")), Own->GetCellText(Highlighted, 4), FString(TEXT("0")));
		int32 Dead = 0;
		int32 Bombs = 0;
		int32 Bots = 0;
		for (int32 Row = 0; Row < T->GetNumRows(); ++Row)
		{
			if (T->GetCellText(Row, 0) == DeadName)
			{
				TestEqual(*(Side + TEXT("DEAD")), T->GetCellText(Row, 1), FString(TEXT("DEAD")));
				TestEqual(*(Side + TEXT("its death")), T->GetCellText(Row, 3), FString(TEXT("1")));
			}
			Dead += T->GetCellText(Row, 1) == TEXT("DEAD") ? 1 : 0;
			Bombs += T->GetCellText(Row, 1) == TEXT("BOMB") ? 1 : 0;
			Bots += T->GetCellText(Row, 4) == TEXT("BOT") ? 1 : 0;
		}
		TestEqual(*(Side + TEXT("one dead")), Dead, 1);
		TestEqual(*(Side + TEXT("the carrier for a terrorist only")), Bombs, ViewerTeam == EShooterTeam::T ? 1 : 0);
		TestEqual(*(Side + TEXT("the bots")), Bots, ViewerTeam == EShooterTeam::T ? 4 : 5);

		// Nothing new: not filled again; a score: filled again, the heading's score.
		const int32 Refreshes = Board->GetNumRefreshes();
		TickFrames(World, 3);
		TestEqual(*(Side + TEXT("nothing new: the same tables")), Board->GetNumRefreshes(), Refreshes);
		GameMode->GetShooterGameState()->AddTeamScore(EShooterTeam::CT);
		TickFrames(World, 1);
		TestEqual(*(Side + TEXT("a score: filled again")), Board->GetNumRefreshes(), Refreshes + 1);
		TestEqual(*(Side + TEXT("the team's score")), Board->GetTeamScoreText(EShooterTeam::CT)->GetText().ToString(),
			FString(TEXT("1")));
		Player->ShowScores(0);
		TickFrames(World, 1);
		TestTrue(*(Side + TEXT("hidden again")), Board->GetPanel()->GetVisibility() == ESlateVisibility::Collapsed);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameHUDBuyMenuTableTest, "ShooterGame.HUD.BuyMenuTable",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameHUDBuyMenuTableTest::RunTest(const FString& Parameters)
{
	// The buy menu's page is a table: the number key, the name (a category, or the item's display name), the price;
	// a category in amber, an item the player cannot afford grey, the pad's line highlighted.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::CT);
	TickUntil(World, *GameMode, EShooterRoundState::Live);
	AShooterHUD* HUD = Cast<AShooterHUD>(Player->MyHUD);
	const UShooterBuyMenuWidget* Menu = HUD != nullptr ? HUD->GetWidgetOfClass<UShooterBuyMenuWidget>() : nullptr;
	if (!TestNotNull("The buy menu", Menu))
	{
		return false;
	}
	Player->BuyMenu();
	TickFrames(World, 1);
	const UTableView* Table = Menu->GetItemTable();
	TestTrue("Open", Player->IsBuyMenuOpen() && Menu->GetPanel()->GetVisibility() == ESlateVisibility::Visible);
	TestEqual("The categories", Table->GetNumRows(), AShooterPlayerController::GetNumBuyMenuCategories());
	TestTrue("1 Pistols >",
		Table->GetCellText(0, 0) == TEXT("1") && Table->GetCellText(0, 1) == TEXT("Pistols") &&
			Table->GetCellText(0, 2) == TEXT(">"));
	TestTrue("A category in amber", Table->GetRowColor(0) == UShooterBuyMenuWidget::CategoryColor);
	TestEqual("The pad's line", Table->GetHighlightedRow(), 0);
	(void)Player->InputKey(EKeys::Three, IE_Pressed, 1.0f, false);
	(void)Player->InputKey(EKeys::Three, IE_Released, 0.0f, false);
	TickFrames(World, 1);
	int32 AwpRow = INDEX_NONE;
	int32 M4Row = INDEX_NONE;
	for (int32 Row = 0; Row < Table->GetNumRows(); ++Row)
	{
		AwpRow = Table->GetCellText(Row, 1) == TEXT("AWP") ? Row : AwpRow;
		M4Row = Table->GetCellText(Row, 1) == TEXT("M4A1") ? Row : M4Row;
	}
	if (!TestTrue("The rifles by their names", AwpRow != INDEX_NONE && M4Row != INDEX_NONE))
	{
		return false;
	}
	TestEqual("Its price", Table->GetCellText(AwpRow, 2), FString(TEXT("$4750")));
	TestTrue("$800 buys no AWP: grey", Table->GetRowColor(AwpRow) == UShooterBuyMenuWidget::UnaffordableColor);
	Player->GetPlayerState<AShooterPlayerState>()->SetMoney(16000, 16000);
	TickFrames(World, 1);
	TestTrue("With the money: white", Table->GetRowColor(AwpRow) == UShooterBuyMenuWidget::AffordableColor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameInputFirePlantsTheBombTest, "ShooterGame.Input.FirePlantsTheBomb",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameInputFirePlantsTheBombTest::RunTest(const FString& Parameters)
{
	// CS: with the C4 drawn in a bomb site the Fire key (the mouse's left button, the pad's right trigger) plants it
	// while held; released early it stops; held for the plant's time it plants. Without the bomb drawn Fire shoots.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::T);
	TickUntil(World, *GameMode, EShooterRoundState::Live);
	AShooterCharacter* Pawn = Cast<AShooterCharacter>(Player->GetPawn());
	AShooterBomb* Bomb = GameMode->GetBomb();
	if (!TestNotNull("The player's pawn", Pawn) || !TestNotNull("The bomb", Bomb))
	{
		return false;
	}
	if (AShooterCharacter* Carrier = Bomb->GetCarrier(); Carrier != nullptr && Carrier != Pawn)
	{
		Carrier->SetCarriedBomb(nullptr);
	}
	Bomb->GiveTo(Pawn);
	Pawn->Reset(FVector(100.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	TickFrames(World, 2);
	AShooterWeapon* Pistol = Pawn->GetWeapon();
	const int32 ShotsBefore = Pistol != nullptr ? Pistol->GetShotsFired() : 0;
	(void)Player->InputKey(EKeys::Five, IE_Pressed, 1.0f, false);
	(void)Player->InputKey(EKeys::Five, IE_Released, 0.0f, false);
	TickFrames(World, 1);
	if (!TestTrue("5 draws the bomb", Pawn->IsBombDrawn()))
	{
		return false;
	}
	(void)Player->InputKey(EKeys::LeftMouseButton, IE_Pressed, 1.0f, false);
	TickFrames(World, 1);
	TestTrue("Fire plants", Pawn->IsPlanting());
	(void)Player->InputKey(EKeys::LeftMouseButton, IE_Released, 0.0f, false);
	TickFrames(World, 1);
	TestFalse("Released early: it stops", Pawn->IsPlanting());
	TestTrue("Still carried", Bomb->GetCarrier() == Pawn);
	(void)Player->InputKey(EKeys::Gamepad_RightTrigger, IE_Pressed, 1.0f, true);
	TickFrames(World, FMath::CeilToInt(Bomb->PlantDuration / FrameTime) + 2);
	(void)Player->InputKey(EKeys::Gamepad_RightTrigger, IE_Released, 0.0f, true);
	TickFrames(World, 1);
	TestTrue("Held: planted", Bomb->GetBombState() == EShooterBombState::Planted);
	TestTrue("No shot fired", Pistol == nullptr || Pistol->GetShotsFired() == ShotsBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameHUDFrameStatsTest, "ShooterGame.HUD.FrameStats",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterGameHUDFrameStatsTest::RunTest(const FString& Parameters)
{
	// The compact frame readout under the radar: the frames' rate and time averaged over FrameStatsRefreshSeconds, in
	// the small font with its shadow, on by default; the player's option hides it. The menus start under it.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterGameMode* GameMode = SetUpMatch(World, 1, 1);
	AShooterPlayerController* Player = AddLocalPlayer(World, *GameMode, EShooterTeam::CT);
	TickUntil(World, *GameMode, EShooterRoundState::Live);
	AShooterHUD* HUD = Cast<AShooterHUD>(Player->MyHUD);
	if (!TestNotNull("The player's HUD", HUD))
	{
		return false;
	}
	const double DeltaTime = FApp::GetDeltaTime();
	FApp::SetDeltaTime(1.0 / 30.0);
	for (int32 Frame = 0; Frame < 20; ++Frame)
	{
		const FHUDFrame Painted(*HUD);
	}
	TestEqual("30 fps, 33.3 ms", HUD->GetFrameStatsText(), FString(TEXT("30 fps  33.3 ms")));
	TestTrue("Under the radar, the menus under it", HUD->GetMenuTop() > HUD->RadarSize + 12.0f + 14.0f);
	Player->GetPersistentUser()->bShowFrameStats = false;
	{
		const FHUDFrame Painted(*HUD);
	}
	TestTrue("The option hides it", HUD->GetFrameStatsText().IsEmpty());
	Player->GetPersistentUser()->bShowFrameStats = true;
	{
		const FHUDFrame Painted(*HUD);
	}
	TestFalse("And shows it again", HUD->GetFrameStatsText().IsEmpty());
	FApp::SetDeltaTime(DeltaTime);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
