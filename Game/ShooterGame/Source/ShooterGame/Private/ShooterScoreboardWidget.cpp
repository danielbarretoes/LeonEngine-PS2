#include "ShooterScoreboardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TableView.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "ShooterCharacter.h"
#include "ShooterGameState.h"
#include "ShooterHUD.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerState.h"

const FName UShooterScoreboardWidget::NameColumn(TEXT("Name"));
const FName UShooterScoreboardWidget::StatusColumn(TEXT("Status"));
const FName UShooterScoreboardWidget::ScoreColumn(TEXT("Score"));
const FName UShooterScoreboardWidget::DeathsColumn(TEXT("Deaths"));
const FName UShooterScoreboardWidget::LatencyColumn(TEXT("Latency"));

namespace
{

	/** The teams' colours (the HUD's, taken as they are, not through the sRGB curve). */
	const FLinearColor CTTextColor(110.0f / 255.0f, 160.0f / 255.0f, 1.0f);
	const FLinearColor TTextColor(1.0f, 190.0f / 255.0f, 90.0f / 255.0f);
	const FLinearColor SpectatorTextColor(0.75f, 0.75f, 0.75f);
	/** How much a dead player's row keeps of its colour. */
	constexpr float DeadDim = 0.55f;
	/** The panel's place on the 640 x 448 canvas (its width is the tables': 456 pixels and the padding). */
	constexpr float PanelLeft = 82.0f;
	constexpr float PanelTop = 56.0f;
	/** The names' column (the tables' other columns are narrower, fixed too). */
	constexpr float NameWidth = 200.0f;

	const FLinearColor& GetTeamTextColor(EShooterTeam Team)
	{
		return Team == EShooterTeam::CT ? CTTextColor : Team == EShooterTeam::T ? TTextColor : SpectatorTextColor;
	}

	/** A player's pawn when it lives, else null. */
	const AShooterCharacter* GetLivingPawn(const AShooterPlayerState& State)
	{
		const AController* Controller = Cast<AController>(State.GetOwner());
		const AShooterCharacter* Pawn =
			Controller != nullptr ? Cast<AShooterCharacter>(Controller->GetPawn()) : nullptr;
		return Pawn != nullptr && Pawn->IsAlive() && !Pawn->IsPendingKillPending() ? Pawn : nullptr;
	}

} // namespace

UShooterScoreboardWidget::UShooterScoreboardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UTableView* UShooterScoreboardWidget::GetTeamTable(EShooterTeam Team) const
{
	return Team == EShooterTeam::CT ? CTTable : Team == EShooterTeam::T ? TTable : nullptr;
}

UTextBlock* UShooterScoreboardWidget::GetTeamScoreText(EShooterTeam Team) const
{
	return Team == EShooterTeam::CT ? CTScoreText : Team == EShooterTeam::T ? TScoreText : nullptr;
}

void UShooterScoreboardWidget::AddTeamHeading(
	UVerticalBox& Box, EShooterTeam Team, UTextBlock*& OutName, UTextBlock*& OutScore)
{
	const AShooterHUD* HUD = Cast<AShooterHUD>(GetOwningHUD());
	UHorizontalBox* Heading = WidgetTree->ConstructWidget<UHorizontalBox>();
	Box.AddChildToVerticalBox(Heading)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 2.0f));
	OutName = WidgetTree->ConstructWidget<UTextBlock>();
	OutName->SetFont(FSlateFontInfo(const_cast<UFont*>(HUD != nullptr ? HUD->GetBoldFont() : nullptr), 14));
	OutName->SetColorAndOpacity(GetTeamTextColor(Team));
	UHorizontalBoxSlot* NameSlot = Heading->AddChildToHorizontalBox(OutName);
	NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	NameSlot->SetVerticalAlignment(VAlign_Bottom);
	NameSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 2.0f));
	OutScore = WidgetTree->ConstructWidget<UTextBlock>();
	OutScore->SetFont(FSlateFontInfo(const_cast<UFont*>(HUD != nullptr ? HUD->GetNumberFont() : nullptr), 24));
	OutScore->SetColorAndOpacity(GetTeamTextColor(Team));
	UHorizontalBoxSlot* ScoreSlot = Heading->AddChildToHorizontalBox(OutScore);
	ScoreSlot->SetHorizontalAlignment(HAlign_Right);
	ScoreSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
}

UTableView* UShooterScoreboardWidget::AddTeamTable(UVerticalBox& Box)
{
	UTableView* Table = WidgetTree->ConstructWidget<UTableView>();
	Table->AddColumn(FTableViewColumn(NameColumn, FText::FromString(TEXT("Name")), NameWidth, HAlign_Left));
	Table->AddColumn(FTableViewColumn(StatusColumn, FText::FromString(TEXT("")), 84.0f, HAlign_Left));
	Table->AddColumn(FTableViewColumn(ScoreColumn, FText::FromString(TEXT("Score")), 52.0f, HAlign_Right));
	Table->AddColumn(FTableViewColumn(DeathsColumn, FText::FromString(TEXT("Deaths")), 56.0f, HAlign_Right));
	Table->AddColumn(FTableViewColumn(LatencyColumn, FText::FromString(TEXT("Latency")), 64.0f, HAlign_Right));
	Table->SetSortMode(ScoreColumn, EColumnSortMode::Descending);
	Table->HeaderTextColor = FLinearColor(0.65f, 0.65f, 0.65f);
	Table->HeaderBackgroundColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.08f);
	Table->RowBackgroundColor = FLinearColor::Transparent;
	Table->AlternateRowBackgroundColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.04f);
	Table->HighlightBackgroundColor = FLinearColor(0.55f, 0.42f, 0.1f, 0.5f);
	Table->CellPadding = FMargin(4.0f, 1.0f);
	Box.AddChildToVerticalBox(Table);
	return Table;
}

void UShooterScoreboardWidget::NativeOnInitialized()
{
	// Canvas > Border (the panel) > VerticalBox > the map's name, then each team's heading and table, then the
	// spectators.
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f));
	Panel->SetPadding(FMargin(10.0f, 8.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetPosition(FVector2D(PanelLeft, PanelTop));
	PanelSlot->SetAutoSize(true);
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Box);
	TitleText = WidgetTree->ConstructWidget<UTextBlock>();
	TitleText->SetColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f));
	TitleText->SetJustification(ETextJustify::Center);
	Box->AddChildToVerticalBox(TitleText)->SetHorizontalAlignment(HAlign_Fill);
	AddTeamHeading(*Box, EShooterTeam::CT, CTNameText, CTScoreText);
	CTTable = AddTeamTable(*Box);
	AddTeamHeading(*Box, EShooterTeam::T, TNameText, TScoreText);
	TTable = AddTeamTable(*Box);
	SpectatorsText = WidgetTree->ConstructWidget<UTextBlock>();
	SpectatorsText->SetColorAndOpacity(SpectatorTextColor);
	Box->AddChildToVerticalBox(SpectatorsText)->SetPadding(FMargin(4.0f, 8.0f, 0.0f, 0.0f));
	Panel->SetVisibility(ESlateVisibility::Collapsed);
}

void UShooterScoreboardWidget::NativeTick(float /*DeltaTime*/)
{
	const AShooterHUD* HUD = Cast<AShooterHUD>(GetOwningHUD());
	const AShooterPlayerController* Controller = HUD != nullptr ? HUD->GetShooterPlayerController() : nullptr;
	const UWorld* World = HUD != nullptr ? HUD->GetWorld() : nullptr;
	const AGameModeBase* GameMode = World != nullptr ? World->GetAuthGameMode() : nullptr;
	const bool bShown = Controller != nullptr && GameMode != nullptr && Controller->IsScoreboardShown();
	const ESlateVisibility PanelVisibility = bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (Panel->GetVisibility() != PanelVisibility)
	{
		Panel->SetVisibility(PanelVisibility);
	}
	if (!bShown)
	{
		return;
	}
	// What the board shows: the scores, the viewer and its team, then each player's team, kills, deaths, life, bomb
	// and bot; the tables are filled again only when it changed.
	const AShooterGameState* State = GameMode->GetGameState<AShooterGameState>();
	const AShooterPlayerState* ViewerState = Controller->GetPlayerState<AShooterPlayerState>();
	NewKey.Reset();
	NewKey.Add(State != nullptr ? State->GetTeamScore(EShooterTeam::CT) : 0);
	NewKey.Add(State != nullptr ? State->GetTeamScore(EShooterTeam::T) : 0);
	NewKey.Add(static_cast<int64>(reinterpret_cast<UPTRINT>(ViewerState)));
	NewKey.Add(ViewerState != nullptr ? static_cast<int64>(ViewerState->GetTeam()) : -1);
	for (const APlayerState* Player : GameMode->GetGameState().GetPlayerArray())
	{
		const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(Player);
		if (ShooterState == nullptr)
		{
			continue;
		}
		const AShooterCharacter* Pawn = GetLivingPawn(*ShooterState);
		NewKey.Add(static_cast<int64>(reinterpret_cast<UPTRINT>(ShooterState)));
		NewKey.Add((static_cast<int64>(ShooterState->GetTeam()) << 8) | (Pawn != nullptr ? 1 : 0) |
			(Pawn != nullptr && Pawn->GetCarriedBomb() != nullptr ? 2 : 0) | (ShooterState->bIsABot ? 4 : 0));
		NewKey.Add((static_cast<int64>(ShooterState->GetKills()) << 32) | uint32(ShooterState->GetDeaths()));
	}
	if (NewKey != ShownKey || NumRefreshes == 0)
	{
		Swap(ShownKey, NewKey);
		Refresh(*Controller);
	}
}

void UShooterScoreboardWidget::Refresh(const AShooterPlayerController& Viewer)
{
	++NumRefreshes;
	const UWorld* World = Viewer.GetWorld();
	const AGameModeBase* GameMode = World != nullptr ? World->GetAuthGameMode() : nullptr;
	if (GameMode == nullptr)
	{
		return;
	}
	const AShooterGameState* State = GameMode->GetGameState<AShooterGameState>();
	const AShooterPlayerState* ViewerState = Viewer.GetPlayerState<AShooterPlayerState>();
	// The bomb's carrier is marked for the terrorists and for a spectator (CS shows it to the terrorists only).
	const EShooterTeam ViewerTeam = ViewerState != nullptr ? ViewerState->GetTeam() : EShooterTeam::None;
	const bool bShowBomb = ViewerTeam != EShooterTeam::CT;
	TitleText->SetText(FText::FromString(World->GetMapName()));
	FString Spectators;
	for (const EShooterTeam Team : {EShooterTeam::CT, EShooterTeam::T})
	{
		// The team's players, fewest deaths first (the order a tie in score keeps), then their rows.
		TArray<const AShooterPlayerState*, TInlineAllocator<16>> Players;
		for (const APlayerState* Player : GameMode->GetGameState().GetPlayerArray())
		{
			const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(Player);
			if (ShooterState != nullptr && ShooterState->GetTeam() == Team)
			{
				Players.Add(ShooterState);
			}
		}
		Players.StableSort(
			[](const AShooterPlayerState& A, const AShooterPlayerState& B) { return A.GetDeaths() < B.GetDeaths(); });
		UTableView& Table = *GetTeamTable(Team);
		Table.ClearRows();
		int32 Highlighted = INDEX_NONE;
		for (const AShooterPlayerState* Player : Players)
		{
			const AShooterCharacter* Pawn = GetLivingPawn(*Player);
			const bool bCarrier = Pawn != nullptr && Pawn->GetCarriedBomb() != nullptr;
			const int32 Row = Table.AddRow();
			Table.SetCellText(Row, 0, Player->GetPlayerName());
			Table.SetCellText(Row, 1,
				Pawn == nullptr             ? FString(TEXT("DEAD"))
					: bCarrier && bShowBomb ? FString(TEXT("BOMB"))
											: FString());
			Table.SetCellText(Row, 2, FString::FromInt(Player->GetKills()));
			Table.SetCellText(Row, 3, FString::FromInt(Player->GetDeaths()));
			// A bot says so (CS); a player's latency (Leon plays locally: none).
			Table.SetCellText(Row, 4, Player->bIsABot ? FString(TEXT("BOT")) : FString(TEXT("0")));
			const FLinearColor& Color = GetTeamTextColor(Team);
			Table.SetRowColor(Row,
				Pawn != nullptr ? Color : FLinearColor(Color.R * DeadDim, Color.G * DeadDim, Color.B * DeadDim, 1.0f));
			if (Player == ViewerState)
			{
				Highlighted = Row;
			}
		}
		Table.SetHighlightedRow(Highlighted);
		Table.SortRows();
		const int32 Score = State != nullptr ? State->GetTeamScore(Team) : 0;
		UTextBlock& Name = *(Team == EShooterTeam::CT ? CTNameText : TNameText);
		Name.SetText(FText::FromString(FString::Printf(TEXT("%s   %d player%s"),
			Team == EShooterTeam::CT ? TEXT("Counter-Terrorists") : TEXT("Terrorists"), Players.Num(),
			Players.Num() == 1 ? TEXT("") : TEXT("s"))));
		GetTeamScoreText(Team)->SetText(FText::FromString(FString::FromInt(Score)));
	}
	for (const APlayerState* Player : GameMode->GetGameState().GetPlayerArray())
	{
		const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(Player);
		if (ShooterState != nullptr && ShooterState->GetTeam() == EShooterTeam::None)
		{
			Spectators += Spectators.IsEmpty() ? TEXT("Spectators: ") : TEXT(", ");
			Spectators += ShooterState->GetPlayerName();
		}
	}
	SpectatorsText->SetText(FText::FromString(Spectators));
	SpectatorsText->SetVisibility(Spectators.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}
